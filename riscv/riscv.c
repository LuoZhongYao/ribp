#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "riscv.h"

#define CAUSE_MISALIGNED_FETCH    0x0
#define CAUSE_FAULT_FETCH         0x1
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT          0x3
#define CAUSE_MISALIGNED_LOAD     0x4
#define CAUSE_FAULT_LOAD          0x5
#define CAUSE_MISALIGNED_STORE    0x6
#define CAUSE_FAULT_STORE         0x7
#define CAUSE_USER_ECALL          0x8
#define CAUSE_SUPERVISOR_ECALL    0x9
#define CAUSE_HYPERVISOR_ECALL    0xa
#define CAUSE_MACHINE_ECALL       0xb
#define CAUSE_FETCH_PAGE_FAULT    0xc
#define CAUSE_LOAD_PAGE_FAULT     0xd
#define CAUSE_STORE_PAGE_FAULT    0xf

/* Note: converted to correct bit position at runtime */
#define CAUSE_INTERRUPT  ((uint32_t)1 << 31) 

/* mstatus CSR */
#define MSTATUS_SPIE_SHIFT 5
#define MSTATUS_MPIE_SHIFT 7
#define MSTATUS_SPP_SHIFT 8
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_FS_SHIFT 13
#define MSTATUS_UXL_SHIFT 32
#define MSTATUS_SXL_SHIFT 34

#define MSTATUS_UIE (1 << 0)
#define MSTATUS_SIE (1 << 1)
#define MSTATUS_HIE (1 << 2)
#define MSTATUS_MIE (1 << 3)
#define MSTATUS_UPIE (1 << 4)
#define MSTATUS_SPIE (1 << MSTATUS_SPIE_SHIFT)
#define MSTATUS_HPIE (1 << 6)
#define MSTATUS_MPIE (1 << MSTATUS_MPIE_SHIFT)
#define MSTATUS_SPP (1 << MSTATUS_SPP_SHIFT)
#define MSTATUS_HPP (3 << 9)
#define MSTATUS_MPP (3 << MSTATUS_MPP_SHIFT)
#define MSTATUS_FS (3 << MSTATUS_FS_SHIFT)
#define MSTATUS_XS (3 << 15)
#define MSTATUS_MPRV (1 << 17)
#define MSTATUS_SUM (1 << 18)
#define MSTATUS_MXR (1 << 19)
//#define MSTATUS_TVM (1 << 20)
//#define MSTATUS_TW (1 << 21)
//#define MSTATUS_TSR (1 << 22)
#define MSTATUS_UXL_MASK ((uint64_t)3 << MSTATUS_UXL_SHIFT)
#define MSTATUS_SXL_MASK ((uint64_t)3 << MSTATUS_SXL_SHIFT)

#define MIP_USIP (1 << 0)
#define MIP_SSIP (1 << 1)
#define MIP_HSIP (1 << 2)
#define MIP_MSIP (1 << 3)
#define MIP_UTIP (1 << 4)
#define MIP_STIP (1 << 5)
#define MIP_HTIP (1 << 6)
#define MIP_MTIP (1 << 7)
#define MIP_UEIP (1 << 8)
#define MIP_SEIP (1 << 9)
#define MIP_HEIP (1 << 10)
#define MIP_MEIP (1 << 11)

static int riscv32_read_u8(struct riscv32_vm *m, uint32_t addr, uint8_t *val)
{
	if ((uint64_t)addr + 0 >= m->memsize)
		return -1;

	*val = m->mem[addr];
	return 0;
}

static int riscv32_read_u16(struct riscv32_vm *m, uint32_t addr, uint16_t *val)
{
	if ((uint64_t)addr + 1 >= m->memsize)
		return -1;

	*val = m->mem[addr] | m->mem[addr + 1] << 8;
	return 0;
}

static int riscv32_read_u32(struct riscv32_vm *m, uint32_t addr, uint32_t *val)
{
	if ((uint64_t)addr + 3 >= m->memsize)
		return -1;

	*val = m->mem[addr] | m->mem[addr + 1] << 8
		| m->mem[addr + 2] << 16 | m->mem[addr + 3] << 24;
	return 0;
}

static int riscv32_write_u8(struct riscv32_vm *m, uint32_t addr, uint8_t val)
{
	if ((uint64_t)addr >= m->memsize)
		return -1;

	m->mem[addr] = val;
	return 0;
}

static int riscv32_write_u16(struct riscv32_vm *m, uint32_t addr, uint16_t val)
{  
	if ((uint64_t)addr + 1 >= m->memsize)
		return -1;

	m->mem[addr] = val & 0xff;
	m->mem[addr + 1] = (val >> 8) & 0xff;
	return 0;
}

static int riscv32_write_u32(struct riscv32_vm *m, uint32_t addr, uint32_t val)
{
	if ((uint64_t)addr + 3 >= m->memsize)
		return -1;

	m->mem[addr] = val & 0xff;
	m->mem[addr + 1] = (val >> 8) & 0xff;
	m->mem[addr + 2] = (val >> 16) & 0xff;
	m->mem[addr + 3] = (val >> 24) & 0xff;
	return 0;
}

static inline int32_t div32(int32_t a, int32_t b)
{
    if (b == 0) {
        return -1;
    } else if (a == ((int32_t)1 << (32 - 1)) && b == -1) {
        return a;
    } else {
        return a / b;
    }
}

static inline uint32_t divu32(uint32_t a, uint32_t b)
{
    if (b == 0) {
        return -1;
    } else {
        return a / b;
    }
}

static inline int32_t rem32(int32_t a, int32_t b)
{
    if (b == 0) {
        return a;
    } else if (a == ((int32_t)1 << (32 - 1)) && b == -1) {
        return 0;
    } else {
        return a % b;
    }
}

static inline uint32_t remu32(uint32_t a, uint32_t b)
{
    if (b == 0) {
        return a;
    } else {
        return a % b;
    }
}

static inline uint32_t mulh32(int32_t a, int32_t b)
{
    return ((int64_t)a * (int64_t)b) >> 32;
}

static inline uint32_t mulhsu32(int32_t a, uint32_t b)
{
    return ((int64_t)a * (int64_t)b) >> 32;
}

static inline uint32_t mulhu32(uint32_t a, uint32_t b)
{
    return ((int64_t)a * (int64_t)b) >> 32;
}

static void raise_exception2(struct riscv32_cpu *c, uint32_t cause, uint32_t tval)
{
	uint32_t causel;

	causel = cause & 0x7fffffff;
	if (cause & CAUSE_INTERRUPT)
		causel |= (uint32_t)1 << (32 - 1);

	c->mcause = causel;
	c->mepc = c->pc;
	c->mtval = tval;
	c->mstatus &= ~MSTATUS_MIE;
	c->pc = c->mtvec;
}

static void raise_exception(struct riscv32_cpu *c, uint32_t cause)
{
    raise_exception2(c, cause, 0);
}

static void handle_mret(struct riscv32_cpu *c)
{
    int mpp, mpie;
    mpp = (c->mstatus >> MSTATUS_MPP_SHIFT) & 3;
    /* set the IE state to previous IE state */
    mpie = (c->mstatus >> MSTATUS_MPIE_SHIFT) & 1;
    c->mstatus = (c->mstatus & ~(1 << mpp)) |
        (mpie << mpp);
    /* set MPIE to 1 */
    c->mstatus |= MSTATUS_MPIE;
    /* set MPP to U */
    c->mstatus &= ~MSTATUS_MPP;
    c->pc = c->mepc;
}

static int csr_read(struct riscv32_cpu *c, uint32_t *pval, uint32_t csr, bool will_write)
{
    uint32_t val;

    if (((csr & 0xc00) == 0xc00) && will_write)
        return -1; /* read-only CSR */
    
    switch(csr) {
    case 0x300:
        val = c->mstatus;
        break;
    case 0x304:
        val = c->mie;
        break;
    case 0x305:
        val = c->mtvec;
        break;
    case 0x340:
        val = c->mscratch;
        break;
    case 0x341:
        val = c->mepc;
        break;
    case 0x342:
        val = c->mcause;
        break;
    case 0x343:
        val = c->mtval;
        break;
    case 0x344:
        val = c->mip;
        break;
    default:
    invalid_csr:
        *pval = 0;
        return -1;
    }
    *pval = val;
    return 0;
}

#define MSTATUS_MASK (MSTATUS_UIE | MSTATUS_SIE | MSTATUS_MIE |      \
                      MSTATUS_UPIE | MSTATUS_SPIE | MSTATUS_MPIE |    \
                      MSTATUS_SPP | MSTATUS_MPP | \
                      MSTATUS_FS | \
                      MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR)
/* return -1 if invalid CSR, 0 if OK, 1 if the interpreter loop must be
   exited (e.g. XLEN was modified), 2 if TLBs have been flushed. */
static int csr_write(struct riscv32_cpu *c, uint32_t csr, uint32_t val)
{
    uint32_t mask;

    switch(csr) {
    case 0x300:
		mask = MSTATUS_MASK & ~MSTATUS_FS;
		c->mstatus = (c->mstatus & ~mask) | (val & mask);
        break;
    case 0x304:
        mask = MIP_MSIP | MIP_MTIP | MIP_SSIP | MIP_STIP | MIP_SEIP;
        c->mie = (c->mie & ~mask) | (val & mask);
        break;
    case 0x305:
        c->mtvec = val & ~3;
        break;
    case 0x340:
        c->mscratch = val;
        break;
    case 0x341:
        c->mepc = val & ~1;
        break;
    case 0x342:
        c->mcause = val;
        break;
    case 0x343:
        c->mtval = val;
        break;
    case 0x344:
        mask = MIP_SSIP | MIP_STIP;
        c->mip = (c->mip & ~mask) | (val & mask);
        break;
    default:
        return -1;
    }
    return 0;
}

int riscv32_cpu_exec(struct riscv32_vm *m)
{
	bool debug = false;
	int32_t imm, cond, err;
	uint32_t addr, val, val2, cause = CAUSE_LOAD_PAGE_FAULT, tval;
	uint32_t opcode, insn, rd, rs1, rs2, funct3;
	struct riscv32_cpu *c = &m->cpu;

	if (riscv32_read_u32(m, c->pc, &insn)) {
		addr = c->pc;
		goto mmu_exception;
	}


	opcode = insn & 0x7f;
	rd = (insn >> 7) & 0x1f;
	rs1 = (insn >> 15) & 0x1f;
	rs2 = (insn >> 20) & 0x1f;

	//printf("insn: %08x opcode = %02x, rs1 = %02x, rs2 = %02x, rd = %02x\n",
	//	insn, opcode, rs1, rs2, rd);

	c->zero = 0;
	switch (opcode) {
	default: goto illegal_insn;

	case 0x37:	/* lui */
		c->reg[rd] = (int32_t)(insn & 0xfffff000);
		c->pc += 4;
	break;

	case 0x17: /* auipc */
		c->reg[rd] = (int32_t)(c->pc + (int32_t)(insn & 0xfffff000));
		c->pc += 4;
	break;

	case 0x6f: /* jal */
		imm = ((insn >> (31 - 20)) & (1 << 20))
			| ((insn >> (21 - 1)) & 0x7fe)
			| ((insn >> (20 - 11)) & (1 << 11))
			| (insn & 0xff000);
		imm = (imm << 11) >> 11;
		c->reg[rd] = c->pc + 4;
		c->pc += imm;
	break;

	case 0x67: /* jalr */
		imm = (int32_t)insn >> 20;
		c->reg[rd] = c->pc + 4;
		c->pc = (int32_t)(c->reg[rs1] + imm) & ~1;
	break;

	case 0x63:
		funct3 = (insn >> 12) & 7;
		switch (funct3 >> 1) {
		case 0: /* beq/bne */
			cond = (c->reg[rs1] == c->reg[rs2]);
		break;

		case 1: /* blt/bge */
			cond = ((int32_t)c->reg[rs1] < (int32_t)c->reg[rs2]);
		break;

		case 2: /* bltu/bgeu */
			cond = (c->reg[rs1] < c->reg[rs2]);
		break;

		default:
			goto illegal_insn;
		break;
		}
		cond ^= (funct3 & 1);
		if (cond) {
			imm = ((insn >> (31 - 12)) & (1 << 12)) |
				((insn >> (25 - 5)) & 0x7e0) |
				((insn >> (8 - 1)) & 0x1e) |
				((insn << (11 - 7)) & (1 << 11));
			imm = (imm << 19) >> 19;
			c->pc += imm;
		} else {
			c->pc += 4;
		}
	break;

	case 0x03: /* load*/
		funct3 = (insn >> 12) & 7;
		imm = (int32_t)insn >> 20;
		addr = c->reg[rs1] + imm;

		tval = addr;
		cause = CAUSE_FAULT_LOAD;
		switch (funct3) {
		case 0: { /* lb */
			uint8_t rval;
			if (riscv32_read_u8(m, addr, &rval))
				goto mmu_exception;
			val = (int8_t)rval;
		} break;

		case 1: { /* lh */
			uint16_t rval;
			if (riscv32_read_u16(m, addr, &rval))
				goto mmu_exception;
			val = (int16_t)rval;
		} break;

		case 2: { /* lw */
			uint32_t rval;
			if (riscv32_read_u32(m, addr, &rval))
				goto mmu_exception;
			val = (int32_t)rval;
		} break;

		case 4: { /* lbu */
			uint8_t rval;
			if (riscv32_read_u8(m, addr, &rval))
				goto mmu_exception;
			val = rval;
		} break;

		case 5: { /* lhu */
			uint16_t rval;
			if (riscv32_read_u16(m, addr, &rval))
				goto mmu_exception;
			val = rval;
		} break;

		default:
			goto illegal_insn;
		}

		c->reg[rd] = val;
		c->pc += 4;
	break;

	case 0x23: /* store */
		funct3 = (insn >> 12) & 7;
		imm = rd | ((insn >> (25 - 5)) & 0xfe0);
		imm = (imm << 20) >> 20;
		addr = c->reg[rs1] + imm;
		val = c->reg[rs2];

		tval = addr;
		cause = CAUSE_FAULT_STORE;
		switch (funct3) {
		case 0: /* sb */
			if (riscv32_write_u8(m, addr, val))
				goto mmu_exception;
		break;

		case 1: /* sh */
			if (riscv32_write_u16(m, addr, val))
				goto mmu_exception;
		break;

		case 2: /* sw */
			if (riscv32_write_u32(m, addr, val))
				goto mmu_exception;
		break;

		default:
			goto illegal_insn;
		}
		c->pc += 4;
	break;

	case 0x13:
		funct3 = (insn >> 12) & 7;
		imm = (int32_t) insn >> 20;
		switch (funct3) {
		case 0: /* addi */
			val = (int32_t)(c->reg[rs1] + imm);
		break;

		case 1: /* slli */
			if ((imm & ~(32 - 1)) != 0)
				goto illegal_insn;
			val = (int32_t)(c->reg[rs1] << (imm & (32 - 1)));
		break;

		case 2: /* slti */
			val = (int32_t)(c->reg[rs1] < imm);
		break;

		case 3: /* sltiu */
			val = c->reg[rs1] < imm;
		break;

		case 4: /* xori */
			val = c->reg[rs1] ^ imm;
		break;

		case 5: /* srli/srai */
			if ((imm & ~((32 - 1) | 0x400)) != 0)
				goto illegal_insn;

			if (imm & 0x400)
				val = (int32_t)c->reg[rs1] >> (imm & (32 - 1));
			else
				val = (int32_t)(c->reg[rs1] >> (imm & (32 - 1)));
		break;

		case 6: /* ori */
			val = c->reg[rs1] | imm;
		break;

		default:
		case 7: /* andi */
			val = c->reg[rs1] & imm;
		break;
		}
		c->reg[rd] = val;
		c->pc += 4;
	break;

	case 0x33:
		imm = insn >> 25;
		val = c->reg[rs1];
		val2 = c->reg[rs2];
		if (imm == 1) {
			funct3 = (insn >> 12) & 7;
			switch(funct3) {
			case 0: /* mul */
				val = (int32_t)((int32_t)val * (int32_t)val2);
			break;
			case 1: /* mulh */
				val = (int32_t)mulh32(val, val2);
			break;
			case 2:/* mulhsu */
				val = (int32_t)mulhsu32(val, val2);
			break;
			case 3:/* mulhu */
				val = (int32_t)mulhu32(val, val2);
			break;
			case 4:/* div */
				val = div32(val, val2);
			break;
			case 5:/* divu */
				val = (int32_t)divu32(val, val2);
			break;
			case 6:/* rem */
				val = rem32(val, val2);
			break;
			case 7:/* remu */
				val = (int32_t)remu32(val, val2);
			break;
			default:
				goto illegal_insn;
			}
		} else {
			if (imm & ~0x20)
				goto illegal_insn;
			funct3 = ((insn >> 12) & 7) | ((insn >> (30 - 3)) & (1 << 3));
			switch(funct3) {
			case 0: /* add */
				val = (int32_t)(val + val2);
			break;
			case 0 | 8: /* sub */
				val = (int32_t)(val - val2);
			break;
			case 1: /* sll */
				val = (int32_t)(val << (val2 & (32 - 1)));
			break;
			case 2: /* slt */
				val = (int32_t)val < (int32_t)val2;
			break;
			case 3: /* sltu */
				val = val < val2;
			break;
			case 4: /* xor */
				val = val ^ val2;
			break;
			case 5: /* srl */
				val = (int32_t)((uint32_t)val >> (val2 & (32 - 1)));
			break;
			case 5 | 8: /* sra */
				val = (int32_t)val >> (val2 & (32 - 1));
			break;
			case 6: /* or */
				val = val | val2;
			break;
			case 7: /* and */
				val = val & val2;
			break;
			default:
				goto illegal_insn;
			}
		}
		c->reg[rd] = val;
		c->pc +=4;
	break;

	case 0x73:
		funct3 = (insn >> 12) & 7;
		imm = insn >> 20;
		if (funct3 & 4)
			val = rs1;
		else
			val = c->reg[rs1];
		funct3 &= 3;
		switch(funct3) {
		case 1: /* csrrw */
			if (csr_read(c, &val2, imm, true))
				goto illegal_insn;
			val2 = (int32_t)val2;
			err = csr_write(c, imm, val);
			if (err < 0)
				goto illegal_insn;
			c->reg[rd] = val2;
			c->pc += 4;
		break;

		case 2: /* csrrs */
		case 3: /* csrrc */
			if (csr_read(c, &val2, imm, (rs1 != 0)))
				goto illegal_insn;
			val2 = (int32_t)val2;
			if (rs1 != 0) {
				if (funct3 == 2)
					val = val2 | val;
				else
					val = val2 & ~val;
				err = csr_write(c, imm, val);
				if (err < 0)
					goto illegal_insn;
			} else {
				err = 0;
			}
			c->reg[rd] = val2;
			c->pc = c->pc + 4;
		break;

		case 0:
			switch(imm) {
			case 0x000: /* ecall */
				if (insn & 0x000fff80)
					goto illegal_insn;
				cause = CAUSE_MACHINE_ECALL;
				goto exception;
			break;

			case 0x001: /* ebreak */
				debug = true;
				if (insn & 0x000fff80)
					goto illegal_insn;
				cause = CAUSE_BREAKPOINT;
				goto exception;
			break;

			case 0x302: /* mret */
				if (insn & 0x000fff80)
					goto illegal_insn;
				handle_mret(c);
			break;

			case 0x105: /* wfi */
				if (insn & 0x00007f80)
					goto illegal_insn;
				c->pc += 4;
			break;

			default: goto illegal_insn;
			break;
			}
		break;
		default: goto illegal_insn;
		}
	break;
	}

the_end:
	c->zero = 0;
	return debug;

illegal_insn:
	cause = CAUSE_ILLEGAL_INSTRUCTION;
	tval = insn;
mmu_exception:
	debug = true;
exception:
	raise_exception2(c, cause, tval) ;
	goto the_end;
}

struct riscv32_vm *riscv32_vm(unsigned memsize)
{
	struct riscv32_vm *vm;
	vm = calloc(1, sizeof(struct riscv32_vm) + memsize);
	if (vm) {
		vm->memsize = memsize;
	}
	return vm;
}

int riscv32_load_rom(struct riscv32_vm *vm, const void *rom, unsigned romsize, unsigned romoff)
{
	if (romsize + romoff > vm->memsize)
		return -1;

	memcpy(vm->mem + romoff, rom, romsize);

	return 0;
}

void *riscv32_mem_map(struct riscv32_vm *vm, unsigned base, unsigned size)
{
	if ((uint64_t)base + size < vm->memsize)
		return vm->mem + base;
	return NULL;
}
