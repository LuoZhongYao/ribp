#include <hostapi.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <riscv.h>
#include <sys/poll.h>

void hostapi_ecall(struct riscv32_vm *vm,
	uint32_t *a0, uint32_t *a1, uint32_t *a2, uint32_t *a3,
	uint32_t *a4, uint32_t *a5, uint32_t *a6, uint32_t *a7)
{
	void *mem = NULL;
	switch (*a0) {
	case HOSTAPI_OPEN:
		mem = riscv32_mem_map(vm, *a1, *a4);
		*a0 = open(mem, *a2, *a3);
	break;

	case HOSTAPI_CLOSE:
		*a0 = close(*a1);
	break;

	case HOSTAPI_READ:
		mem = riscv32_mem_map(vm, *a2, *a3);
		*a0 = read(*a1, mem, *a3);
	break;

	case HOSTAPI_WRITE:
		mem = riscv32_mem_map(vm, *a2, *a3);
		*a0 = write(*a1, mem, *a3);
	break;

	case HOSTAPI_SEEK:
		*a0 = lseek(*a1, *a2, *a3);
	break;

	case HOSTAPI_POLL:
		mem = riscv32_mem_map(vm, *a1, *a2 * sizeof(struct pollfd));
		*a0 = poll(mem, *a2, *a3);
	break;

	default:
		*a0 = -1;
	break;
	}
}
