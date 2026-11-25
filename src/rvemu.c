#include "rvemu.h"

#include <stdlib.h>
#include <string.h>

static uint32_t read_le(const uint8_t *p, unsigned n) {
    uint32_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= (uint32_t)p[i] << (8u * i);
    return v;
}

static void write_le(uint8_t *p, unsigned n, uint32_t v) {
    for (unsigned i = 0; i < n; ++i) p[i] = (uint8_t)(v >> (8u * i));
}

static int32_t sext(uint32_t value, unsigned bits) {
    uint32_t sign = 1u << (bits - 1u);
    return (int32_t)((value ^ sign) - sign);
}

static bool range_ok(const rvemu *cpu, uint32_t address, size_t n) {
    return (uint64_t)address + n <= cpu->memory_size;
}

bool rvemu_init(rvemu *cpu, size_t memory_size) {
    if (!cpu || !memory_size || memory_size > UINT32_MAX) return false;
    memset(cpu, 0, sizeof(*cpu));
    cpu->memory = calloc(memory_size, 1);
    if (!cpu->memory) return false;
    cpu->memory_size = memory_size;
    cpu->last_status = RVEMU_OK;
    return true;
}

void rvemu_destroy(rvemu *cpu) {
    if (!cpu) return;
    free(cpu->memory);
    memset(cpu, 0, sizeof(*cpu));
}

bool rvemu_load_bytes(rvemu *cpu, uint32_t address, const void *data, size_t size) {
    if (!cpu || !cpu->memory || (!data && size) || !range_ok(cpu, address, size)) return false;
    if (size) memcpy(cpu->memory + address, data, size);
    return true;
}

static rvemu_status fail(rvemu *cpu, rvemu_status status) {
    cpu->last_status = status;
    return status;
}

rvemu_status rvemu_step(rvemu *cpu) {
    if (!cpu || !cpu->memory) return RVEMU_ERR_MEMORY;
    if (cpu->halted) return RVEMU_HALT;
    if (cpu->pc & 3u) return fail(cpu, RVEMU_ERR_ALIGNMENT);
    if (!range_ok(cpu, cpu->pc, 4)) return fail(cpu, RVEMU_ERR_MEMORY);

    const uint32_t insn = read_le(cpu->memory + cpu->pc, 4);
    const uint32_t opcode = insn & 0x7fu;
    const unsigned rd = (insn >> 7) & 31u, f3 = (insn >> 12) & 7u;
    const unsigned rs1 = (insn >> 15) & 31u, rs2 = (insn >> 20) & 31u;
    const unsigned f7 = insn >> 25;
    const uint32_t a = cpu->x[rs1], b = cpu->x[rs2];
    uint32_t next = cpu->pc + 4u, result = 0;
    bool write_rd = false;

    switch (opcode) {
    case 0x37: /* LUI */
        result = insn & 0xfffff000u; write_rd = true; break;
    case 0x17: /* AUIPC */
        result = cpu->pc + (insn & 0xfffff000u); write_rd = true; break;
    case 0x6f: { /* JAL */
        uint32_t imm = ((insn >> 31) << 20) | (((insn >> 12) & 0xffu) << 12) |
                       (((insn >> 20) & 1u) << 11) | (((insn >> 21) & 0x3ffu) << 1);
        result = next; write_rd = true; next = cpu->pc + (uint32_t)sext(imm, 21); break;
    }
    case 0x67: { /* JALR */
        if (f3 != 0) return fail(cpu, RVEMU_ERR_INSTRUCTION);
        uint32_t target = (a + (uint32_t)sext(insn >> 20, 12)) & ~1u;
        result = next; write_rd = true; next = target; break;
    }
    case 0x63: { /* branches */
        uint32_t imm = ((insn >> 31) << 12) | (((insn >> 7) & 1u) << 11) |
                       (((insn >> 25) & 0x3fu) << 5) | (((insn >> 8) & 0xfu) << 1);
        bool take;
        switch (f3) {
        case 0: take = a == b; break; case 1: take = a != b; break;
        case 4: take = (int32_t)a < (int32_t)b; break; case 5: take = (int32_t)a >= (int32_t)b; break;
        case 6: take = a < b; break; case 7: take = a >= b; break;
        default: return fail(cpu, RVEMU_ERR_INSTRUCTION);
        }
        if (take) next = cpu->pc + (uint32_t)sext(imm, 13);
        break;
    }
    case 0x03: { /* loads */
        uint32_t address = a + (uint32_t)sext(insn >> 20, 12);
        unsigned width; bool sign;
        switch (f3) { case 0: width=1; sign=true; break; case 1: width=2; sign=true; break;
        case 2: width=4; sign=true; break; case 4: width=1; sign=false; break;
        case 5: width=2; sign=false; break; default: return fail(cpu, RVEMU_ERR_INSTRUCTION); }
        if (!range_ok(cpu, address, width)) return fail(cpu, RVEMU_ERR_MEMORY);
        uint32_t value = read_le(cpu->memory + address, width);
        if (sign && width < 4) value = (uint32_t)sext(value, width * 8u);
        result=value; write_rd=true; break;
    }
    case 0x23: { /* stores */
        uint32_t imm = ((insn >> 25) << 5) | ((insn >> 7) & 31u);
        uint32_t address = a + (uint32_t)sext(imm, 12);
        unsigned width = f3 == 0 ? 1u : f3 == 1 ? 2u : f3 == 2 ? 4u : 0u;
        if (!width) return fail(cpu, RVEMU_ERR_INSTRUCTION);
        if (!range_ok(cpu, address, width)) return fail(cpu, RVEMU_ERR_MEMORY);
        write_le(cpu->memory + address, width, b); break;
    }
    case 0x13: { /* immediate ALU */
        uint32_t imm = (uint32_t)sext(insn >> 20, 12);
        switch (f3) {
        case 0: result=a+imm; break; case 2: result=(int32_t)a < (int32_t)imm; break;
        case 3: result=a < imm; break; case 4: result=a^imm; break; case 6: result=a|imm; break; case 7: result=a&imm; break;
        case 1: if (f7 != 0) return fail(cpu, RVEMU_ERR_INSTRUCTION); result=a << ((insn>>20)&31u); break;
        case 5: if (f7 == 0) result=a >> ((insn>>20)&31u); else if (f7 == 0x20) result=(uint32_t)((int32_t)a >> ((insn>>20)&31u)); else return fail(cpu, RVEMU_ERR_INSTRUCTION); break;
        }
        write_rd=true; break;
    }
    case 0x33: { /* register ALU */
        switch (f3) {
        case 0: if (f7==0) result=a+b; else if (f7==0x20) result=a-b; else return fail(cpu,RVEMU_ERR_INSTRUCTION); break;
        case 1: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=a<<(b&31u); break;
        case 2: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=(int32_t)a<(int32_t)b; break;
        case 3: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=a<b; break;
        case 4: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=a^b; break;
        case 5: if(f7==0) result=a>>(b&31u); else if(f7==0x20) result=(uint32_t)((int32_t)a>>(b&31u)); else return fail(cpu,RVEMU_ERR_INSTRUCTION); break;
        case 6: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=a|b; break;
        case 7: if(f7) return fail(cpu,RVEMU_ERR_INSTRUCTION); result=a&b; break;
        }
        write_rd=true; break;
    }
    case 0x0f: /* FENCE is a no-op in this single-hart interpreter. */
        if (f3 != 0 && f3 != 1) return fail(cpu, RVEMU_ERR_INSTRUCTION);
        break;
    case 0x73: /* A small, useful execution environment: EBREAK stops. */
        if (insn == 0x00100073u) { cpu->pc = next; cpu->halted=true; cpu->x[0]=0; return fail(cpu,RVEMU_HALT); }
        return fail(cpu, RVEMU_ERR_INSTRUCTION);
    default:
        return fail(cpu, RVEMU_ERR_INSTRUCTION);
    }
    if (write_rd && rd != 0) cpu->x[rd] = result;
    cpu->pc = next;
    cpu->x[0] = 0;
    cpu->last_status = RVEMU_OK;
    return RVEMU_OK;
}

rvemu_status rvemu_run(rvemu *cpu, uint64_t max_steps, uint64_t *steps_run) {
    uint64_t n = 0;
    while (n < max_steps) {
        rvemu_status s = rvemu_step(cpu);
        if (s != RVEMU_OK) { if (steps_run) *steps_run=n; return s; }
        ++n;
    }
    if (steps_run) *steps_run=n;
    return fail(cpu, RVEMU_ERR_LIMIT);
}

const char *rvemu_status_string(rvemu_status s) {
    switch (s) {
    case RVEMU_OK: return "ok"; case RVEMU_HALT: return "halted";
    case RVEMU_ERR_MEMORY: return "memory access out of range";
    case RVEMU_ERR_INSTRUCTION: return "illegal or unsupported instruction";
    case RVEMU_ERR_ALIGNMENT: return "instruction address misaligned";
    case RVEMU_ERR_LIMIT: return "instruction limit reached";
    }
    return "unknown status";
}
