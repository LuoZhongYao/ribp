#define _GNU_SOURCE
#include "riscv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <errno.h>
#include <stdbool.h>
#include <debug.h>

void debug_exception_handler (struct riscv32_vm *vm, bool intr);
int mkptms(const char* target, mode_t perm)
{
	int fdm, oflags;
	struct termios ts;

#if __QNX__
	int slave;
	char slavename[128];
	if(openpty(&fdm, &slave, slavename, NULL, NULL) < 0) {
		BLOGE("openpty %s: %s", target, strerror(errno));
		return -1;
	}
#else
	char *slavename;
	fdm = open("/dev/ptmx", O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fdm < 0) {
		BLOGE("/dev/ptmx: %s", strerror(errno));
		return -1;
	}
	grantpt(fdm); /* change permission of slave */
	unlockpt(fdm); /* unlock slave */
	slavename = ptsname(fdm); /* get name of slave */
	open(slavename, O_NONBLOCK);
#endif

	unlink(target);
	oflags = fcntl(fdm, F_GETFL);
	oflags |= (O_NONBLOCK | O_NOCTTY);
	fcntl(fdm, F_SETFL, oflags);

	if(0 == tcgetattr(fdm, &ts)) {
		cfmakeraw(&ts);
		tcsetattr (fdm, TCSANOW, &ts);
	}

	if(0 != chmod(slavename, perm)) {
		BLOGE("chmod %s: %s", slavename, strerror(errno));
	}

	if (0 != symlink(slavename, target)) {
		BLOGE("symlink %s %s: %s", slavename, target, strerror(errno));
		goto __err;
	}
	return fdm;
__err:
	close(fdm);
	return -1;
}

int dfd = -1;

int getDebugChar(void)
{
	int rn = -1;
	char ch;
	if (dfd != 1)
		rn = read(dfd, &ch, 1);
//	BLOGD("getchar: %c\n", ch);
	return rn != 1 ? -1 : ch;
}

void putDebugChar(char ch)
{
	//BLOGD("putchar: %c\n", ch);
	if (dfd != -1)
		write(dfd, &ch, 1);
}

int main(int argc, char **argv)
{
	int c, fd, rn, off = 0;
	bool debug = false;
	const char *romfile = "rom.bin", *stub = NULL;
	char buf[4096];
	unsigned memsize = 1024 * 400;
	struct riscv32_vm *vm;

	while (-1 != (c = getopt(argc, argv, "r:m:d:"))) {
		switch (c) {
		case 'r':
			romfile = optarg;
		break;

		case 'd':
			stub = optarg;
		break;

		case 'm':
			memsize = strtoul(optarg, NULL, 0);
		break;
		}
	}

	fd = open(romfile, O_RDONLY);
	if (fd < 0) {
		perror(romfile);
		return 1;
	}

	vm = riscv32_vm(memsize + 8);

	while (0 < (rn = read(fd, buf, sizeof(buf)))) {
		riscv32_load_rom(vm, buf, rn, off);
		off += rn;
	}

	/* store hostapi */
	off = (off + 3) & ~3;
	riscv32_load_rom(vm, "\x73\x00\x00\x00\x67\x80\x00\x00", 8, off);
	vm->cpu.pc = 0;
	vm->cpu.sp = memsize;
	vm->cpu.fp = memsize;
	vm->cpu.a0 = off;

	close(fd);

	if (stub != NULL) {
		dfd = mkptms(stub, 0666);
	}
	if (dfd >= 0)
		debug_exception_handler(vm, false);

	while (1) {
		if (debug || 0x03 == getDebugChar()) {
			debug_exception_handler(vm, debug);
		}

		debug = riscv32_cpu_exec(vm);
	}

	return 0;
}
