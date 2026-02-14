#include "decode.h"
#include "rv32_defs.h"

using namespace rv32;

static DecodedInstr decode32(uint32_t instr) {
    DecodedInstr d;
    d.raw = instr;

    uint32_t op = opcode(instr);

    switch (op) {
    case OP_LUI:
        d.type = InstrType::LUI;
        d.rd   = rd(instr);
        d.imm  = imm_u(instr);
        break;

    case OP_AUIPC:
        d.type = InstrType::AUIPC;
        d.rd   = rd(instr);
        d.imm  = imm_u(instr);
        break;

    case OP_JAL:
        d.type = InstrType::JAL;
        d.rd   = rd(instr);
        d.imm  = imm_j(instr);
        break;

    case OP_JALR:
        d.type = InstrType::JALR;
        d.rd   = rd(instr);
        d.rs1  = rs1(instr);
        d.imm  = imm_i(instr);
        break;

    case OP_BRANCH: {
        d.rs1 = rs1(instr);
        d.rs2 = rs2(instr);
        d.imm = imm_b(instr);
        switch (funct3(instr)) {
        case F3_BEQ:  d.type = InstrType::BEQ;  break;
        case F3_BNE:  d.type = InstrType::BNE;  break;
        case F3_BLT:  d.type = InstrType::BLT;  break;
        case F3_BGE:  d.type = InstrType::BGE;  break;
        case F3_BLTU: d.type = InstrType::BLTU; break;
        case F3_BGEU: d.type = InstrType::BGEU; break;
        default:      d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_LOAD: {
        d.rd  = rd(instr);
        d.rs1 = rs1(instr);
        d.imm = imm_i(instr);
        switch (funct3(instr)) {
        case F3_LB:  d.type = InstrType::LB;  break;
        case F3_LH:  d.type = InstrType::LH;  break;
        case F3_LW:  d.type = InstrType::LW;  break;
        case F3_LBU: d.type = InstrType::LBU; break;
        case F3_LHU: d.type = InstrType::LHU; break;
        default:     d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_STORE: {
        d.rs1 = rs1(instr);
        d.rs2 = rs2(instr);
        d.imm = imm_s(instr);
        switch (funct3(instr)) {
        case F3_SB: d.type = InstrType::SB; break;
        case F3_SH: d.type = InstrType::SH; break;
        case F3_SW: d.type = InstrType::SW; break;
        default:    d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_IMM: {
        d.rd  = rd(instr);
        d.rs1 = rs1(instr);
        d.imm = imm_i(instr);
        uint32_t f3 = funct3(instr);
        uint32_t f7 = funct7(instr);
        switch (f3) {
        case F3_ADD_SUB: d.type = InstrType::ADDI;  break;
        case F3_SLT:     d.type = InstrType::SLTI;  break;
        case F3_SLTU:    d.type = InstrType::SLTIU; break;
        case F3_XOR:     d.type = InstrType::XORI;  break;
        case F3_OR:      d.type = InstrType::ORI;   break;
        case F3_AND:     d.type = InstrType::ANDI;  break;
        case F3_SLL:
            d.type = (f7 == F7_NORMAL) ? InstrType::SLLI : InstrType::ILLEGAL;
            d.imm = rs2(instr); // shamt
            break;
        case F3_SRL_SRA:
            if (f7 == F7_NORMAL)    d.type = InstrType::SRLI;
            else if (f7 == F7_ALT)  d.type = InstrType::SRAI;
            else                    d.type = InstrType::ILLEGAL;
            d.imm = rs2(instr); // shamt
            break;
        default: d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_REG: {
        d.rd  = rd(instr);
        d.rs1 = rs1(instr);
        d.rs2 = rs2(instr);
        uint32_t f3 = funct3(instr);
        uint32_t f7 = funct7(instr);

        if (f7 == F7_MULDIV) {
            switch (f3) {
            case F3_MUL:    d.type = InstrType::MUL;    break;
            case F3_MULH:   d.type = InstrType::MULH;   break;
            case F3_MULHSU: d.type = InstrType::MULHSU; break;
            case F3_MULHU:  d.type = InstrType::MULHU;  break;
            case F3_DIV:    d.type = InstrType::DIV;     break;
            case F3_DIVU:   d.type = InstrType::DIVU;    break;
            case F3_REM:    d.type = InstrType::REM;     break;
            case F3_REMU:   d.type = InstrType::REMU;    break;
            }
        } else if (f7 == F7_NORMAL) {
            switch (f3) {
            case F3_ADD_SUB: d.type = InstrType::ADD;  break;
            case F3_SLL:     d.type = InstrType::SLL;  break;
            case F3_SLT:     d.type = InstrType::SLT;  break;
            case F3_SLTU:    d.type = InstrType::SLTU; break;
            case F3_XOR:     d.type = InstrType::XOR;  break;
            case F3_SRL_SRA: d.type = InstrType::SRL;  break;
            case F3_OR:      d.type = InstrType::OR;   break;
            case F3_AND:     d.type = InstrType::AND;  break;
            }
        } else if (f7 == F7_ALT) {
            switch (f3) {
            case F3_ADD_SUB: d.type = InstrType::SUB; break;
            case F3_SRL_SRA: d.type = InstrType::SRA; break;
            default:         d.type = InstrType::ILLEGAL;
            }
        } else {
            d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_AMO: {
        d.rd  = rd(instr);
        d.rs1 = rs1(instr);
        d.rs2 = rs2(instr);
        uint32_t f5 = funct5(instr);
        if (funct3(instr) != 0b010) { // must be W (funct3=010)
            d.type = InstrType::ILLEGAL;
            break;
        }
        switch (f5) {
        case F5_LR:      d.type = InstrType::LR_W;      break;
        case F5_SC:      d.type = InstrType::SC_W;       break;
        case F5_AMOSWAP: d.type = InstrType::AMOSWAP_W;  break;
        case F5_AMOADD:  d.type = InstrType::AMOADD_W;   break;
        case F5_AMOXOR:  d.type = InstrType::AMOXOR_W;   break;
        case F5_AMOAND:  d.type = InstrType::AMOAND_W;   break;
        case F5_AMOOR:   d.type = InstrType::AMOOR_W;    break;
        case F5_AMOMIN:  d.type = InstrType::AMOMIN_W;   break;
        case F5_AMOMAX:  d.type = InstrType::AMOMAX_W;   break;
        case F5_AMOMINU: d.type = InstrType::AMOMINU_W;  break;
        case F5_AMOMAXU: d.type = InstrType::AMOMAXU_W;  break;
        default:         d.type = InstrType::ILLEGAL;
        }
        break;
    }

    case OP_FENCE:
        d.type = (funct3(instr) == F3_FENCEI) ? InstrType::FENCEI : InstrType::FENCE;
        break;

    case OP_SYSTEM: {
        uint32_t f3 = funct3(instr);
        if (f3 == F3_PRIV) {
            uint32_t f7v = funct7(instr);
            uint32_t f12v = funct12(instr);
            if (f7v == F7_SFENCE_VMA) {
                d.type = InstrType::SFENCE_VMA;
                d.rs1 = rs1(instr);
                d.rs2 = rs2(instr);
            } else {
                switch (f12v) {
                case F12_ECALL:  d.type = InstrType::ECALL;  break;
                case F12_EBREAK: d.type = InstrType::EBREAK; break;
                case F12_MRET:   d.type = InstrType::MRET;   break;
                case F12_SRET:   d.type = InstrType::SRET;   break;
                case F12_URET:   d.type = InstrType::URET;   break;
                case F12_WFI:    d.type = InstrType::WFI;    break;
                default:         d.type = InstrType::ILLEGAL;
                }
            }
        } else {
            d.rd  = rd(instr);
            d.rs1 = rs1(instr);
            d.csr = funct12(instr);
            d.imm = csr_zimm(instr); // for CSRR*I variants
            switch (f3) {
            case F3_CSRRW:  d.type = InstrType::CSRRW;  break;
            case F3_CSRRS:  d.type = InstrType::CSRRS;  break;
            case F3_CSRRC:  d.type = InstrType::CSRRC;  break;
            case F3_CSRRWI: d.type = InstrType::CSRRWI; break;
            case F3_CSRRSI: d.type = InstrType::CSRRSI; break;
            case F3_CSRRCI: d.type = InstrType::CSRRCI; break;
            default:        d.type = InstrType::ILLEGAL;
            }
        }
        break;
    }

    default:
        d.type = InstrType::ILLEGAL;
    }

    return d;
}

// RV32C compressed instruction expansion

// Little helper, building register index from 3-bit compressed encoding (maps to x8-x15)
static uint32_t creg(uint32_t bits) { return bits + 8; }

uint32_t expand_compressed(uint16_t ci) {
    uint32_t op  = ci & 0x3;
    uint32_t f3  = (ci >> 13) & 0x7;

    switch (op) {
    case 0b00: // Quadrant 0
        switch (f3) {
        case 0b000: { // C.ADDI4SPN -> addi rd', x2, nzuimm
            // nzuimm[5:4|9:6|2|3] from ci[12:7|6|5]
            uint32_t nzuimm = ((ci >> 1) & 0x3C0) | ((ci >> 7) & 0x30) |
                              ((ci >> 2) & 0x8)   | ((ci >> 4) & 0x4);
            if (nzuimm == 0) return 0;
            uint32_t rdp = creg((ci >> 2) & 0x7);
            return (nzuimm << 20) | (2 << 15) | (0b000 << 12) | (rdp << 7) | OP_IMM;
        }
        case 0b010: { // C.LW -> lw rd', offset(rs1')
            uint32_t rs1p = creg((ci >> 7) & 0x7);
            uint32_t rdp  = creg((ci >> 2) & 0x7);
            uint32_t off  = ((ci >> 7) & 0x38) | ((ci >> 4) & 0x4) | ((ci << 1) & 0x40);
            return (off << 20) | (rs1p << 15) | (0b010 << 12) | (rdp << 7) | OP_LOAD;
        }
        case 0b110: { // C.SW -> sw rs2', offset(rs1')
            uint32_t rs1p = creg((ci >> 7) & 0x7);
            uint32_t rs2p = creg((ci >> 2) & 0x7);
            uint32_t off  = ((ci >> 7) & 0x38) | ((ci >> 4) & 0x4) | ((ci << 1) & 0x40);
            uint32_t imm_s_hi = (off >> 5) & 0x7F;
            uint32_t imm_s_lo = off & 0x1F;
            return (imm_s_hi << 25) | (rs2p << 20) | (rs1p << 15) |
                   (0b010 << 12) | (imm_s_lo << 7) | OP_STORE;
        }
        default: return 0;
        }

    case 0b01: // Quadrant 1
        switch (f3) {
        case 0b000: { // C.ADDI / C.NOP -> addi rd, rd, nzimm
            uint32_t r = rd(ci << 0) >> 0; // rd is bits [11:7]
            r = (ci >> 7) & 0x1F;
            int32_t nzimm = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
            if (nzimm & 0x20) nzimm |= ~0x3F; // sign extend
            return (static_cast<uint32_t>(nzimm) << 20) | (r << 15) | (0b000 << 12) | (r << 7) | OP_IMM;
        }
        case 0b001: { // C.JAL -> jal x1, offset
            int32_t off = ((ci >> 1) & 0x800) | ((ci >> 7) & 0x10) |
                          ((ci >> 1) & 0x300) | ((ci << 2) & 0x400) |
                          ((ci >> 1) & 0x40)  | ((ci << 1) & 0x80)  |
                          ((ci >> 2) & 0xE)   | ((ci << 3) & 0x20);
            if (off & 0x800) off |= ~0xFFF;
            uint32_t imm20 = ((off >> 20) & 0x1) << 31 |
                             ((off >> 1) & 0x3FF) << 21 |
                             ((off >> 11) & 0x1) << 20 |
                             ((off >> 12) & 0xFF) << 12;
            return imm20 | (1 << 7) | OP_JAL;
        }
        case 0b010: { // C.LI -> addi rd, x0, imm
            uint32_t r = (ci >> 7) & 0x1F;
            int32_t imm = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
            if (imm & 0x20) imm |= ~0x3F;
            return (static_cast<uint32_t>(imm) << 20) | (0 << 15) | (0b000 << 12) | (r << 7) | OP_IMM;
        }
        case 0b011: { // C.LUI / C.ADDI16SP
            uint32_t r = (ci >> 7) & 0x1F;
            if (r == 2) {
                // C.ADDI16SP -> addi x2, x2, nzimm
                int32_t nzimm = ((ci >> 3) & 0x200) | ((ci >> 2) & 0x10) |
                                ((ci << 1) & 0x40)  | ((ci << 4) & 0x180) |
                                ((ci << 3) & 0x20);
                if (nzimm & 0x200) nzimm |= ~0x3FF;
                if (nzimm == 0) return 0;
                return (static_cast<uint32_t>(nzimm) << 20) | (2 << 15) | (0b000 << 12) | (2 << 7) | OP_IMM;
            } else {
                // C.LUI -> lui rd, nzimm
                int32_t nzimm = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
                if (nzimm & 0x20) nzimm |= ~0x3F;
                if (nzimm == 0) return 0;
                return static_cast<uint32_t>(nzimm << 12) | (r << 7) | OP_LUI;
            }
        }
        case 0b100: { // C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR, C.AND
            uint32_t rdp = creg((ci >> 7) & 0x7);
            uint32_t sub = (ci >> 10) & 0x3;
            switch (sub) {
            case 0b00: { // C.SRLI
                uint32_t shamt = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
                return (F7_NORMAL << 25) | (shamt << 20) | (rdp << 15) |
                       (F3_SRL_SRA << 12) | (rdp << 7) | OP_IMM;
            }
            case 0b01: { // C.SRAI
                uint32_t shamt = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
                return (F7_ALT << 25) | (shamt << 20) | (rdp << 15) |
                       (F3_SRL_SRA << 12) | (rdp << 7) | OP_IMM;
            }
            case 0b10: { // C.ANDI
                int32_t imm = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
                if (imm & 0x20) imm |= ~0x3F;
                return (static_cast<uint32_t>(imm) << 20) | (rdp << 15) |
                       (F3_AND << 12) | (rdp << 7) | OP_IMM;
            }
            case 0b11: {
                uint32_t rs2p = creg((ci >> 2) & 0x7);
                uint32_t sub2 = ((ci >> 5) & 0x3);
                bool hi = (ci >> 12) & 0x1;
                if (!hi) {
                    switch (sub2) {
                    case 0b00: // C.SUB
                        return (F7_ALT << 25) | (rs2p << 20) | (rdp << 15) |
                               (F3_ADD_SUB << 12) | (rdp << 7) | OP_REG;
                    case 0b01: // C.XOR
                        return (F7_NORMAL << 25) | (rs2p << 20) | (rdp << 15) |
                               (F3_XOR << 12) | (rdp << 7) | OP_REG;
                    case 0b10: // C.OR
                        return (F7_NORMAL << 25) | (rs2p << 20) | (rdp << 15) |
                               (F3_OR << 12) | (rdp << 7) | OP_REG;
                    case 0b11: // C.AND
                        return (F7_NORMAL << 25) | (rs2p << 20) | (rdp << 15) |
                               (F3_AND << 12) | (rdp << 7) | OP_REG;
                    }
                }
                return 0;
            }
            }
            break;
        }
        case 0b101: { // C.J -> jal x0, offset
            int32_t off = ((ci >> 1) & 0x800) | ((ci >> 7) & 0x10) |
                          ((ci >> 1) & 0x300) | ((ci << 2) & 0x400) |
                          ((ci >> 1) & 0x40)  | ((ci << 1) & 0x80)  |
                          ((ci >> 2) & 0xE)   | ((ci << 3) & 0x20);
            if (off & 0x800) off |= ~0xFFF;
            uint32_t imm20 = ((off >> 20) & 0x1) << 31 |
                             ((off >> 1) & 0x3FF) << 21 |
                             ((off >> 11) & 0x1) << 20 |
                             ((off >> 12) & 0xFF) << 12;
            return imm20 | (0 << 7) | OP_JAL;
        }
        case 0b110: { // C.BEQZ -> beq rs1', x0, offset
            uint32_t rs1p = creg((ci >> 7) & 0x7);
            int32_t off = ((ci >> 4) & 0x100) | ((ci >> 7) & 0x18) |
                          ((ci << 1) & 0xC0)  | ((ci >> 2) & 0x6)  |
                          ((ci << 3) & 0x20);
            if (off & 0x100) off |= ~0x1FF;
            uint32_t imm_hi = ((off >> 12) & 0x1) << 6 | ((off >> 5) & 0x3F);
            uint32_t imm_lo = ((off >> 1) & 0xF) << 1 | ((off >> 11) & 0x1);
            return (imm_hi << 25) | (0 << 20) | (rs1p << 15) |
                   (F3_BEQ << 12) | (imm_lo << 7) | OP_BRANCH;
        }
        case 0b111: { // C.BNEZ -> bne rs1', x0, offset
            uint32_t rs1p = creg((ci >> 7) & 0x7);
            int32_t off = ((ci >> 4) & 0x100) | ((ci >> 7) & 0x18) |
                          ((ci << 1) & 0xC0)  | ((ci >> 2) & 0x6)  |
                          ((ci << 3) & 0x20);
            if (off & 0x100) off |= ~0x1FF;
            uint32_t imm_hi = ((off >> 12) & 0x1) << 6 | ((off >> 5) & 0x3F);
            uint32_t imm_lo = ((off >> 1) & 0xF) << 1 | ((off >> 11) & 0x1);
            return (imm_hi << 25) | (0 << 20) | (rs1p << 15) |
                   (F3_BNE << 12) | (imm_lo << 7) | OP_BRANCH;
        }
        }
        break;

    case 0b10: // Quadrant 2
        switch (f3) {
        case 0b000: { // C.SLLI -> slli rd, rd, shamt
            uint32_t r = (ci >> 7) & 0x1F;
            uint32_t shamt = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1F);
            return (F7_NORMAL << 25) | (shamt << 20) | (r << 15) |
                   (F3_SLL << 12) | (r << 7) | OP_IMM;
        }
        case 0b010: { // C.LWSP -> lw rd, offset(x2)
            uint32_t r = (ci >> 7) & 0x1F;
            if (r == 0) return 0;
            uint32_t off = ((ci >> 7) & 0x20) | ((ci >> 2) & 0x1C) | ((ci << 4) & 0xC0);
            return (off << 20) | (2 << 15) | (0b010 << 12) | (r << 7) | OP_LOAD;
        }
        case 0b100: {
            uint32_t r1 = (ci >> 7) & 0x1F;
            uint32_t r2 = (ci >> 2) & 0x1F;
            bool hi = (ci >> 12) & 0x1;
            if (!hi) {
                if (r2 == 0) {
                    // C.JR -> jalr x0, rs1, 0
                    if (r1 == 0) return 0;
                    return (r1 << 15) | (0 << 12) | (0 << 7) | OP_JALR;
                } else {
                    // C.MV -> add rd, x0, rs2
                    return (F7_NORMAL << 25) | (r2 << 20) | (0 << 15) |
                           (F3_ADD_SUB << 12) | (r1 << 7) | OP_REG;
                }
            } else {
                if (r2 == 0) {
                    if (r1 == 0) {
                        // C.EBREAK -> ebreak
                        return (F12_EBREAK << 20) | OP_SYSTEM;
                    } else {
                        // C.JALR -> jalr x1, rs1, 0
                        return (r1 << 15) | (0 << 12) | (1 << 7) | OP_JALR;
                    }
                } else {
                    // C.ADD -> add rd, rd, rs2
                    return (F7_NORMAL << 25) | (r2 << 20) | (r1 << 15) |
                           (F3_ADD_SUB << 12) | (r1 << 7) | OP_REG;
                }
            }
        }
        case 0b110: { // C.SWSP -> sw rs2, offset(x2)
            uint32_t r2  = (ci >> 2) & 0x1F;
            uint32_t off = ((ci >> 7) & 0x3C) | ((ci >> 1) & 0xC0);
            uint32_t imm_hi = (off >> 5) & 0x7F;
            uint32_t imm_lo = off & 0x1F;
            return (imm_hi << 25) | (r2 << 20) | (2 << 15) |
                   (0b010 << 12) | (imm_lo << 7) | OP_STORE;
        }
        default: return 0;
        }
        break;
    }

    return 0;
}

DecodedInstr decode(uint32_t instr) {
    if ((instr & 0x3) != 0x3) {
        // Compressed instruction (bits[1:0] != 11)
        uint32_t expanded = expand_compressed(static_cast<uint16_t>(instr & 0xFFFF));
        if (expanded == 0) {
            DecodedInstr d;
            d.raw = instr & 0xFFFF;
            d.compressed = true;
            d.type = InstrType::ILLEGAL;
            return d;
        }
        DecodedInstr d = decode32(expanded);
        d.raw = instr & 0xFFFF;
        d.compressed = true;
        return d;
    }

    return decode32(instr);
}
