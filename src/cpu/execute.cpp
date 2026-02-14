#include "execute.h"
#include "rv32m.h"
#include "rv32a.h"
#include "rv32_defs.h"

using namespace rv32;

static ExecResult make_exception(uint32_t cause, uint32_t tval = 0) {
    return {true, cause, tval};
}

ExecResult execute(CPUState& s, const DecodedInstr& d) {
    uint32_t rs1 = s.get_regu(d.rs1);
    int32_t  rs1s = s.get_reg(d.rs1);
    uint32_t rs2 = s.get_regu(d.rs2);
    int32_t  rs2s = s.get_reg(d.rs2);
    uint32_t imm = static_cast<uint32_t>(d.imm);

    s.next_pc = s.pc + d.instr_len();

    switch (d.type) {

    case InstrType::LUI:
        s.set_reg(d.rd, d.imm);
        break;
    case InstrType::AUIPC:
        s.set_reg(d.rd, static_cast<int32_t>(s.pc + imm));
        break;

    case InstrType::JAL:
        s.set_reg(d.rd, static_cast<int32_t>(s.pc + d.instr_len()));
        s.next_pc = s.pc + imm;
        break;
    case InstrType::JALR: {
        uint32_t target = (rs1 + imm) & ~1u;
        s.set_reg(d.rd, static_cast<int32_t>(s.pc + d.instr_len()));
        s.next_pc = target;
        break;
    }

    case InstrType::BEQ:  if (rs1 == rs2) s.next_pc = s.pc + imm; break;
    case InstrType::BNE:  if (rs1 != rs2) s.next_pc = s.pc + imm; break;
    case InstrType::BLT:  if (rs1s < rs2s) s.next_pc = s.pc + imm; break;
    case InstrType::BGE:  if (rs1s >= rs2s) s.next_pc = s.pc + imm; break;
    case InstrType::BLTU: if (rs1 < rs2) s.next_pc = s.pc + imm; break;
    case InstrType::BGEU: if (rs1 >= rs2) s.next_pc = s.pc + imm; break;

    case InstrType::LB: {
        uint32_t addr = rs1 + imm;
        s.set_reg(d.rd, static_cast<int8_t>(s.mem.read(addr, 1)));
        break;
    }
    case InstrType::LH: {
        uint32_t addr = rs1 + imm;
        if (addr & 1) return make_exception(CAUSE_MISALIGNED_LOAD, addr);
        s.set_reg(d.rd, static_cast<int16_t>(s.mem.read(addr, 2)));
        break;
    }
    case InstrType::LW: {
        uint32_t addr = rs1 + imm;
        if (addr & 3) return make_exception(CAUSE_MISALIGNED_LOAD, addr);
        s.set_reg(d.rd, static_cast<int32_t>(s.mem.read(addr, 4)));
        break;
    }
    case InstrType::LBU: {
        uint32_t addr = rs1 + imm;
        s.set_reg(d.rd, static_cast<int32_t>(s.mem.read(addr, 1) & 0xFF));
        break;
    }
    case InstrType::LHU: {
        uint32_t addr = rs1 + imm;
        if (addr & 1) return make_exception(CAUSE_MISALIGNED_LOAD, addr);
        s.set_reg(d.rd, static_cast<int32_t>(s.mem.read(addr, 2) & 0xFFFF));
        break;
    }

    case InstrType::SB: {
        uint32_t addr = rs1 + imm;
        s.mem.write(addr, rs2 & 0xFF, 1);
        s.lr_sc.clear();
        break;
    }
    case InstrType::SH: {
        uint32_t addr = rs1 + imm;
        if (addr & 1) return make_exception(CAUSE_MISALIGNED_STORE, addr);
        s.mem.write(addr, rs2 & 0xFFFF, 2);
        s.lr_sc.clear();
        break;
    }
    case InstrType::SW: {
        uint32_t addr = rs1 + imm;
        if (addr & 3) return make_exception(CAUSE_MISALIGNED_STORE, addr);
        s.mem.write(addr, rs2, 4);
        s.lr_sc.clear();
        break;
    }

    case InstrType::ADDI:  s.set_reg(d.rd, rs1s + d.imm); break;
    case InstrType::SLTI:  s.set_reg(d.rd, rs1s < d.imm ? 1 : 0); break;
    case InstrType::SLTIU: s.set_reg(d.rd, rs1 < imm ? 1 : 0); break;
    case InstrType::XORI:  s.set_reg(d.rd, rs1s ^ d.imm); break;
    case InstrType::ORI:   s.set_reg(d.rd, rs1s | d.imm); break;
    case InstrType::ANDI:  s.set_reg(d.rd, rs1s & d.imm); break;
    case InstrType::SLLI:  s.set_reg(d.rd, static_cast<int32_t>(rs1 << (d.imm & 0x1F))); break;
    case InstrType::SRLI:  s.set_reg(d.rd, static_cast<int32_t>(rs1 >> (d.imm & 0x1F))); break;
    case InstrType::SRAI:  s.set_reg(d.rd, rs1s >> (d.imm & 0x1F)); break;

    case InstrType::ADD:  s.set_reg(d.rd, rs1s + rs2s); break;
    case InstrType::SUB:  s.set_reg(d.rd, rs1s - rs2s); break;
    case InstrType::SLL:  s.set_reg(d.rd, static_cast<int32_t>(rs1 << (rs2 & 0x1F))); break;
    case InstrType::SLT:  s.set_reg(d.rd, rs1s < rs2s ? 1 : 0); break;
    case InstrType::SLTU: s.set_reg(d.rd, rs1 < rs2 ? 1 : 0); break;
    case InstrType::XOR:  s.set_reg(d.rd, rs1s ^ rs2s); break;
    case InstrType::SRL:  s.set_reg(d.rd, static_cast<int32_t>(rs1 >> (rs2 & 0x1F))); break;
    case InstrType::SRA:  s.set_reg(d.rd, rs1s >> (rs2 & 0x1F)); break;
    case InstrType::OR:   s.set_reg(d.rd, rs1s | rs2s); break;
    case InstrType::AND:  s.set_reg(d.rd, rs1s & rs2s); break;

    case InstrType::MUL:    s.set_reg(d.rd, static_cast<int32_t>(rv32m::mul(rs1, rs2))); break;
    case InstrType::MULH:   s.set_reg(d.rd, static_cast<int32_t>(rv32m::mulh(rs1, rs2))); break;
    case InstrType::MULHSU: s.set_reg(d.rd, static_cast<int32_t>(rv32m::mulhsu(rs1, rs2))); break;
    case InstrType::MULHU:  s.set_reg(d.rd, static_cast<int32_t>(rv32m::mulhu(rs1, rs2))); break;
    case InstrType::DIV:    s.set_reg(d.rd, static_cast<int32_t>(rv32m::div(rs1, rs2))); break;
    case InstrType::DIVU:   s.set_reg(d.rd, static_cast<int32_t>(rv32m::divu(rs1, rs2))); break;
    case InstrType::REM:    s.set_reg(d.rd, static_cast<int32_t>(rv32m::rem(rs1, rs2))); break;
    case InstrType::REMU:   s.set_reg(d.rd, static_cast<int32_t>(rv32m::remu(rs1, rs2))); break;

    case InstrType::LR_W: {
        uint32_t addr = rs1;
        if (addr & 3) return make_exception(CAUSE_MISALIGNED_LOAD, addr);
        s.set_reg(d.rd, static_cast<int32_t>(s.mem.read(addr, 4)));
        s.lr_sc.set(addr);
        break;
    }
    case InstrType::SC_W: {
        uint32_t addr = rs1;
        if (addr & 3) return make_exception(CAUSE_MISALIGNED_STORE, addr);
        if (s.lr_sc.check(addr)) {
            s.mem.write(addr, rs2, 4);
            s.set_reg(d.rd, 0);
        } else {
            s.set_reg(d.rd, 1);
        }
        s.lr_sc.clear();
        break;
    }

    case InstrType::AMOSWAP_W:
    case InstrType::AMOADD_W:
    case InstrType::AMOXOR_W:
    case InstrType::AMOAND_W:
    case InstrType::AMOOR_W:
    case InstrType::AMOMIN_W:
    case InstrType::AMOMAX_W:
    case InstrType::AMOMINU_W:
    case InstrType::AMOMAXU_W: {
        uint32_t addr = rs1;
        if (addr & 3) return make_exception(CAUSE_MISALIGNED_STORE, addr);
        uint32_t mem_val = s.mem.read(addr, 4);
        s.set_reg(d.rd, static_cast<int32_t>(mem_val));
        uint32_t result;
        switch (d.type) {
        case InstrType::AMOSWAP_W: result = rv32a::amo_swap(mem_val, rs2); break;
        case InstrType::AMOADD_W:  result = rv32a::amo_add(mem_val, rs2); break;
        case InstrType::AMOXOR_W:  result = rv32a::amo_xor(mem_val, rs2); break;
        case InstrType::AMOAND_W:  result = rv32a::amo_and(mem_val, rs2); break;
        case InstrType::AMOOR_W:   result = rv32a::amo_or(mem_val, rs2); break;
        case InstrType::AMOMIN_W:  result = rv32a::amo_min(mem_val, rs2); break;
        case InstrType::AMOMAX_W:  result = rv32a::amo_max(mem_val, rs2); break;
        case InstrType::AMOMINU_W: result = rv32a::amo_minu(mem_val, rs2); break;
        case InstrType::AMOMAXU_W: result = rv32a::amo_maxu(mem_val, rs2); break;
        default: result = 0; break;
        }
        s.mem.write(addr, result, 4);
        break;
    }

    case InstrType::CSRRW: {
        uint32_t old_val;
        if (d.rd != 0) {
            if (!s.csr.read(d.csr, s.priv, old_val))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
            s.set_reg(d.rd, static_cast<int32_t>(old_val));
        }
        if (!s.csr.write(d.csr, s.priv, rs1))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        break;
    }
    case InstrType::CSRRS: {
        uint32_t old_val;
        if (!s.csr.read(d.csr, s.priv, old_val))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.set_reg(d.rd, static_cast<int32_t>(old_val));
        if (d.rs1 != 0) {
            if (!s.csr.write(d.csr, s.priv, old_val | rs1))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        }
        break;
    }
    case InstrType::CSRRC: {
        uint32_t old_val;
        if (!s.csr.read(d.csr, s.priv, old_val))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.set_reg(d.rd, static_cast<int32_t>(old_val));
        if (d.rs1 != 0) {
            if (!s.csr.write(d.csr, s.priv, old_val & ~rs1))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        }
        break;
    }
    case InstrType::CSRRWI: {
        uint32_t old_val;
        uint32_t zimm = d.rs1;
        if (d.rd != 0) {
            if (!s.csr.read(d.csr, s.priv, old_val))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
            s.set_reg(d.rd, static_cast<int32_t>(old_val));
        }
        if (!s.csr.write(d.csr, s.priv, zimm))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        break;
    }
    case InstrType::CSRRSI: {
        uint32_t old_val;
        uint32_t zimm = d.rs1;
        if (!s.csr.read(d.csr, s.priv, old_val))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.set_reg(d.rd, static_cast<int32_t>(old_val));
        if (zimm != 0) {
            if (!s.csr.write(d.csr, s.priv, old_val | zimm))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        }
        break;
    }
    case InstrType::CSRRCI: {
        uint32_t old_val;
        uint32_t zimm = d.rs1;
        if (!s.csr.read(d.csr, s.priv, old_val))
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.set_reg(d.rd, static_cast<int32_t>(old_val));
        if (zimm != 0) {
            if (!s.csr.write(d.csr, s.priv, old_val & ~zimm))
                return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        }
        break;
    }

    case InstrType::ECALL:
        switch (s.priv) {
        case PRV_U: return make_exception(CAUSE_ECALL_U);
        case PRV_S: return make_exception(CAUSE_ECALL_S);
        default:    return make_exception(CAUSE_ECALL_M);
        }

    case InstrType::EBREAK:
        return make_exception(CAUSE_BREAKPOINT, s.pc);

    case InstrType::MRET:
        if (s.priv < PRV_M) return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.next_pc = s.csr.mepc;
        {
            uint32_t mpp = (s.csr.mstatus >> MSTATUS_MPP_SHIFT) & 0x3;
            bool mpie = (s.csr.mstatus & MSTATUS_MPIE) != 0;
            s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_MIE) | (mpie ? MSTATUS_MIE : 0);
            s.csr.mstatus |= MSTATUS_MPIE;
            s.csr.mstatus &= ~MSTATUS_MPP_MASK;
            s.priv = static_cast<uint8_t>(mpp);
        }
        break;

    case InstrType::SRET:
        if (s.priv < PRV_S) return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        if (s.csr.mstatus & MSTATUS_TSR && s.priv == PRV_S)
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        s.next_pc = s.csr.sepc;
        {
            bool spp = (s.csr.mstatus & MSTATUS_SPP) != 0;
            bool spie = (s.csr.mstatus & MSTATUS_SPIE) != 0;
            s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_SIE) | (spie ? MSTATUS_SIE : 0);
            s.csr.mstatus |= MSTATUS_SPIE;
            s.csr.mstatus &= ~MSTATUS_SPP;
            s.priv = spp ? PRV_S : PRV_U;
        }
        break;

    case InstrType::URET:
        return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);

    case InstrType::WFI:
        break;

    case InstrType::SFENCE_VMA:
        if (s.priv < PRV_S) return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        if (s.csr.mstatus & MSTATUS_TVM && s.priv == PRV_S)
            return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
        break;

    case InstrType::FENCE:
    case InstrType::FENCEI:
        break;

    case InstrType::ILLEGAL:
        return make_exception(CAUSE_ILLEGAL_INSTR, d.raw);
    }

    return {};
}
