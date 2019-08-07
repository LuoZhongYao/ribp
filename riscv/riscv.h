/*************************************************
 * Anthor  : LuoZhongYao@gmail.com
 * Modified: 2019/08/06
 ************************************************/
#ifndef __RISCV_H__
#define __RISCV_H__
#include <stdint.h>

struct riscv32_cpu {
	uint32_t mtvec;
	uint32_t mepc;
	uint32_t mcause;
	uint32_t mie;
	uint32_t mip;
	uint32_t mtval;
	uint32_t mscratch;
	uint32_t mstatus;

	union {
		uint32_t reg[32];
		struct {
			uint32_t zero;
			uint32_t ra;
			uint32_t sp;
			uint32_t gp;
			uint32_t tp;
			uint32_t t0;
			uint32_t t1;
			uint32_t t2;
			uint32_t fp;
			uint32_t s1;
			uint32_t a0;
			uint32_t a1;
			uint32_t a2;
			uint32_t a3;
			uint32_t a4;
			uint32_t a5;
			uint32_t a6;
			uint32_t a7;
			uint32_t s2;
			uint32_t s3;
			uint32_t s4;
			uint32_t s5;
			uint32_t s6;
			uint32_t s7;
			uint32_t s8;
			uint32_t s9;
			uint32_t s10;
			uint32_t s11;
			uint32_t t3;
			uint32_t t4;
			uint32_t t5;
			uint32_t t6;
		};
	};
	uint32_t pc;
};

struct riscv32_vm
{
	struct riscv32_cpu cpu;
	unsigned memsize;
	uint8_t	mem[0];
};

struct riscv32_vm *riscv32_vm(unsigned memsize);
int riscv32_cpu_exec(struct riscv32_vm *vm);
int riscv32_load_rom(struct riscv32_vm *vm, const void *rom, unsigned romsize, unsigned romoff);
void *riscv32_mem_map(struct riscv32_vm *vm, unsigned base, unsigned size);

#endif /* __RISCV_H__*/

