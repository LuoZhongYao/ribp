#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <riscv.h>
#include <stdbool.h>
#include <debug.h>

extern void  putDebugChar(char c);
extern char getDebugChar(void);	/* read and return a single char */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 1024
static const char hexchars[]="0123456789abcdef";
static int hex (unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

/* scan for the sequence $<data>#<checksum>     */
static char *getpacket (void)
{
	char *buffer = &remcomInBuffer[0];
	unsigned char checksum;
	unsigned char xmitcsum;
	int count;
	char ch;

	while (1) {
		/* wait around for the start character, ignore all other characters */
		while ((ch = getDebugChar ()) != '$')
			;

retry:
		checksum = 0;
		xmitcsum = -1;
		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < BUFMAX - 1) {
			ch = getDebugChar ();
			if (ch == '$')
				goto retry;
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}
		buffer[count] = 0;

		if (ch == '#') {
			ch = getDebugChar ();
			xmitcsum = hex (ch) << 4;
			ch = getDebugChar ();
			xmitcsum += hex (ch);

			if (checksum != xmitcsum) {
				putDebugChar ('-');	/* failed checksum */
			} else {
				putDebugChar ('+');	/* successful transfer */

				/* if a sequence char is present, reply the sequence ID */
				if (buffer[2] == ':') {
					putDebugChar (buffer[0]);
					putDebugChar (buffer[1]);

					return &buffer[3];
				}

				return &buffer[0];
			}
		}
	}
}

/* send the packet in buffer.  */
static void putpacket (char *buffer)
{
	unsigned char checksum;
	int count;
	unsigned char ch;

	/*  $<packet info>#<checksum>. */
	do {
		putDebugChar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			putDebugChar(ch);
			checksum += ch;
			count += 1;
		}

		putDebugChar('#');
		putDebugChar(hexchars[checksum >> 4]);
		putDebugChar(hexchars[checksum & 0xf]);

	} while (getDebugChar() != '+');
}

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */

static char *value_unavailable(char *buf, int count)
{
	while(count-- > 0) {
		*buf++ = 'x';
		*buf++ = 'x';
	}
	*buf = 0;
	return buf;
}

static char * mem2hex (char *mem, char *buf, int count, int may_fault)
{
	unsigned char ch;
	while (count-- > 0) {
		ch = *mem++;
		if (mem_err)
			return 0;
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch & 0xf];
	}

	*buf = 0;
	return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char * hex2mem (char *buf, char *mem, int count, int may_fault)
{
	int i;
	unsigned char ch;

	for (i=0; i<count; i++) {
		ch = hex(*buf++) << 4;
		ch |= hex(*buf++);
		*mem++ = ch;
		if (mem_err)
			return 0;
	}
	return mem;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int hexToInt(char **ptr, unsigned *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return (numChars);
}

/*
 * This function does all command procesing for interfacing to gdb.  It
 * returns 1 if you should skip the instruction at the trap address, 0
 * otherwise.
 */

#define REG_NUM(reg)	((offsetof(struct riscv32_cpu, reg) - offsetof(struct riscv32_cpu, zero)) >> 2)
void debug_exception_handler (struct riscv32_vm *vm, bool intr)
{
	int sigval = 5;
	unsigned addr;
	unsigned length;
	char *ptr;

	struct riscv32_cpu *c = &vm->cpu;
	ptr = remcomOutBuffer;

	*ptr++ = 'T';
	*ptr++ = hexchars[sigval >> 4];
	*ptr++ = hexchars[sigval & 0xf];

	*ptr++ = hexchars[REG_NUM(fp) >> 4];
	*ptr++ = hexchars[REG_NUM(fp) & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char*)&c->fp, ptr, 4, 0); /* FP */
	*ptr++ = ';';

	*ptr++ = hexchars[REG_NUM(sp) >> 4];
	*ptr++ = hexchars[REG_NUM(sp) & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char *)&c->sp, ptr, 4, 0);
	*ptr++ = ';';

	*ptr++ = hexchars[REG_NUM(pc) >> 4];
	*ptr++ = hexchars[REG_NUM(pc) & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char *)(intr ? &c->mepc : &c->pc), ptr, 4, 0);
	*ptr++ = ';';

	*ptr++ = 0;

	putpacket(remcomOutBuffer);

	while (1) {
		remcomOutBuffer[0] = 0;

		ptr = getpacket();
		switch (*ptr++) {
		case '?':
			remcomOutBuffer[0] = 'S';
			remcomOutBuffer[1] = hexchars[sigval >> 4];
			remcomOutBuffer[2] = hexchars[sigval & 0xf];
			remcomOutBuffer[3] = 0;
		break;

		case 'q':
			switch (*ptr++) {
			case 'S':
				switch(*ptr++) {
				case 'u':
					strcpy(remcomOutBuffer, "PacketSize=1024");
				break;
				case 'y':
					strcpy(remcomOutBuffer, "OK");
				break;
				}
			break;
			//case 'O':	/*Offset*/
			//	strcpy(remcomOutBuffer, "Text=0;Data=0;Bss=0;");
			//break;
			}
		break;

		case 'H':
			strcpy(remcomOutBuffer, "OK");
		break;

		case 'd':		/* toggle debug flag */
		break;

		/* return the value of the CPU registers */
		case 'g': {
			mem2hex((void*)&c->zero, remcomOutBuffer, 33 * 4, 0);
		}
		break;

		/* set the value of the CPU registers - return OK */
		case 'G': {
			strcpy(remcomOutBuffer,"OK");
		}
		break;

		case 'm':	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
			/* Try to read %x,%x.  */

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length))
			{
				void *mem = riscv32_mem_map(vm, addr, length);
				if (mem && mem2hex(mem, remcomOutBuffer, length, 1))
					break;

				strcpy (remcomOutBuffer, "E03");
			}
			else
				strcpy(remcomOutBuffer,"E01");
		break;

		case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
			/* Try to read '%x,%x:'.  */

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)
				&& *ptr++ == ':')
			{
				void *mem = riscv32_mem_map(vm, addr, length);

				if (mem && hex2mem(ptr, mem, length, 1))
					strcpy(remcomOutBuffer, "OK");
				else
					strcpy(remcomOutBuffer, "E03");
			}
			else
				strcpy(remcomOutBuffer, "E02");
		break;

		case 'c':    /* cAA..AA    Continue at address AA..AA(optional) */
			/* try to read optional parameter, pc unchanged if no parm */

			return;

			/* kill the program */
		case 'k' :		/* do nothing */
		break;
#if 0
		case 't':		/* Test feature */
			asm (" std %f30,[%sp]");
		break;
#endif
		case 'r':		/* Reset */
		break;
		}			/* switch */

		/* reply to the request */
		putpacket(remcomOutBuffer);
	}
}
