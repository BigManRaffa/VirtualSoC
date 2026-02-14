#ifndef GAMINGCPU_VP_RV32_DEFS_H
#define GAMINGCPU_VP_RV32_DEFS_H

#include <cstdint>

// RV32IMAC opcode/CSR/cause constants. Equivalent to rtl/cpu/pkg/rv32_pkg.sv.

namespace rv32
{

    //  Major opcodes (bits [6:0])
    constexpr uint32_t OP_LUI = 0b0110111;
    constexpr uint32_t OP_AUIPC = 0b0010111;
    constexpr uint32_t OP_JAL = 0b1101111;
    constexpr uint32_t OP_JALR = 0b1100111;
    constexpr uint32_t OP_BRANCH = 0b1100011;
    constexpr uint32_t OP_LOAD = 0b0000011;
    constexpr uint32_t OP_STORE = 0b0100011;
    constexpr uint32_t OP_IMM = 0b0010011;
    constexpr uint32_t OP_REG = 0b0110011;
    constexpr uint32_t OP_FENCE = 0b0001111;
    constexpr uint32_t OP_SYSTEM = 0b1110011;
    constexpr uint32_t OP_AMO = 0b0101111;

    //  funct3 for branches
    constexpr uint32_t F3_BEQ = 0b000;
    constexpr uint32_t F3_BNE = 0b001;
    constexpr uint32_t F3_BLT = 0b100;
    constexpr uint32_t F3_BGE = 0b101;
    constexpr uint32_t F3_BLTU = 0b110;
    constexpr uint32_t F3_BGEU = 0b111;

    //  funct3 for loads
    constexpr uint32_t F3_LB = 0b000;
    constexpr uint32_t F3_LH = 0b001;
    constexpr uint32_t F3_LW = 0b010;
    constexpr uint32_t F3_LBU = 0b100;
    constexpr uint32_t F3_LHU = 0b101;

    //  funct3 for stores
    constexpr uint32_t F3_SB = 0b000;
    constexpr uint32_t F3_SH = 0b001;
    constexpr uint32_t F3_SW = 0b010;

    //  funct3 for ALU immediate / register
    constexpr uint32_t F3_ADD_SUB = 0b000;
    constexpr uint32_t F3_SLL = 0b001;
    constexpr uint32_t F3_SLT = 0b010;
    constexpr uint32_t F3_SLTU = 0b011;
    constexpr uint32_t F3_XOR = 0b100;
    constexpr uint32_t F3_SRL_SRA = 0b101;
    constexpr uint32_t F3_OR = 0b110;
    constexpr uint32_t F3_AND = 0b111;

    //  funct7
    constexpr uint32_t F7_NORMAL = 0b0000000;
    constexpr uint32_t F7_ALT = 0b0100000;    // SUB, SRA
    constexpr uint32_t F7_MULDIV = 0b0000001; // M extension

    //  funct3 for M extension
    constexpr uint32_t F3_MUL = 0b000;
    constexpr uint32_t F3_MULH = 0b001;
    constexpr uint32_t F3_MULHSU = 0b010;
    constexpr uint32_t F3_MULHU = 0b011;
    constexpr uint32_t F3_DIV = 0b100;
    constexpr uint32_t F3_DIVU = 0b101;
    constexpr uint32_t F3_REM = 0b110;
    constexpr uint32_t F3_REMU = 0b111;

    //  funct5 for A extension (bits [31:27])
    constexpr uint32_t F5_LR = 0b00010;
    constexpr uint32_t F5_SC = 0b00011;
    constexpr uint32_t F5_AMOSWAP = 0b00001;
    constexpr uint32_t F5_AMOADD = 0b00000;
    constexpr uint32_t F5_AMOXOR = 0b00100;
    constexpr uint32_t F5_AMOAND = 0b01100;
    constexpr uint32_t F5_AMOOR = 0b01000;
    constexpr uint32_t F5_AMOMIN = 0b10000;
    constexpr uint32_t F5_AMOMAX = 0b10100;
    constexpr uint32_t F5_AMOMINU = 0b11000;
    constexpr uint32_t F5_AMOMAXU = 0b11100;

    //  funct3 for SYSTEM
    constexpr uint32_t F3_PRIV = 0b000;
    constexpr uint32_t F3_CSRRW = 0b001;
    constexpr uint32_t F3_CSRRS = 0b010;
    constexpr uint32_t F3_CSRRC = 0b011;
    constexpr uint32_t F3_CSRRWI = 0b101;
    constexpr uint32_t F3_CSRRSI = 0b110;
    constexpr uint32_t F3_CSRRCI = 0b111;

    //  SYSTEM funct12 (imm[11:0]) for our privileged instructions
    constexpr uint32_t F12_ECALL = 0x000;
    constexpr uint32_t F12_EBREAK = 0x001;
    constexpr uint32_t F12_URET = 0x002;
    constexpr uint32_t F12_SRET = 0x102;
    constexpr uint32_t F12_MRET = 0x302;
    constexpr uint32_t F12_WFI = 0x105;

    // funct7 for SFENCE.VMA
    constexpr uint32_t F7_SFENCE_VMA = 0b0001001;

    //  funct3 for FENCE
    constexpr uint32_t F3_FENCE = 0b000;
    constexpr uint32_t F3_FENCEI = 0b001;

    //  CSR addresses
    // User-level
    constexpr uint16_t CSR_CYCLE = 0xC00;
    constexpr uint16_t CSR_TIME = 0xC01;
    constexpr uint16_t CSR_INSTRET = 0xC02;
    constexpr uint16_t CSR_CYCLEH = 0xC80;
    constexpr uint16_t CSR_TIMEH = 0xC81;
    constexpr uint16_t CSR_INSTRETH = 0xC82;

    // Supervisor-level
    constexpr uint16_t CSR_SSTATUS = 0x100;
    constexpr uint16_t CSR_SIE = 0x104;
    constexpr uint16_t CSR_STVEC = 0x105;
    constexpr uint16_t CSR_SCOUNTEREN = 0x106;
    constexpr uint16_t CSR_SSCRATCH = 0x140;
    constexpr uint16_t CSR_SEPC = 0x141;
    constexpr uint16_t CSR_SCAUSE = 0x142;
    constexpr uint16_t CSR_STVAL = 0x143;
    constexpr uint16_t CSR_SIP = 0x144;
    constexpr uint16_t CSR_SATP = 0x180;

    // Machine-level
    constexpr uint16_t CSR_MSTATUS = 0x300;
    constexpr uint16_t CSR_MISA = 0x301;
    constexpr uint16_t CSR_MEDELEG = 0x302;
    constexpr uint16_t CSR_MIDELEG = 0x303;
    constexpr uint16_t CSR_MIE = 0x304;
    constexpr uint16_t CSR_MTVEC = 0x305;
    constexpr uint16_t CSR_MCOUNTEREN = 0x306;
    constexpr uint16_t CSR_MSCRATCH = 0x340;
    constexpr uint16_t CSR_MEPC = 0x341;
    constexpr uint16_t CSR_MCAUSE = 0x342;
    constexpr uint16_t CSR_MTVAL = 0x343;
    constexpr uint16_t CSR_MIP = 0x344;

    // Machine counters
    constexpr uint16_t CSR_MCYCLE = 0xB00;
    constexpr uint16_t CSR_MINSTRET = 0xB02;
    constexpr uint16_t CSR_MCYCLEH = 0xB80;
    constexpr uint16_t CSR_MINSTRETH = 0xB82;

    // Machine info (read-only)
    constexpr uint16_t CSR_MVENDORID = 0xF11;
    constexpr uint16_t CSR_MARCHID = 0xF12;
    constexpr uint16_t CSR_MIMPID = 0xF13;
    constexpr uint16_t CSR_MHARTID = 0xF14;

    //  mstatus bit positions
    constexpr uint32_t MSTATUS_MIE = 1 << 3;
    constexpr uint32_t MSTATUS_SIE = 1 << 1;
    constexpr uint32_t MSTATUS_MPIE = 1 << 7;
    constexpr uint32_t MSTATUS_SPIE = 1 << 5;
    constexpr uint32_t MSTATUS_MPP_SHIFT = 11;
    constexpr uint32_t MSTATUS_MPP_MASK = 0x3 << MSTATUS_MPP_SHIFT;
    constexpr uint32_t MSTATUS_SPP = 1 << 8;
    constexpr uint32_t MSTATUS_MPRV = 1 << 17;
    constexpr uint32_t MSTATUS_SUM = 1 << 18;
    constexpr uint32_t MSTATUS_MXR = 1 << 19;
    constexpr uint32_t MSTATUS_TVM = 1 << 20;
    constexpr uint32_t MSTATUS_TW = 1 << 21;
    constexpr uint32_t MSTATUS_TSR = 1 << 22;

    //  mip / mie bit positions
    constexpr uint32_t MIP_SSIP = 1 << 1;
    constexpr uint32_t MIP_MSIP = 1 << 3;
    constexpr uint32_t MIP_STIP = 1 << 5;
    constexpr uint32_t MIP_MTIP = 1 << 7;
    constexpr uint32_t MIP_SEIP = 1 << 9;
    constexpr uint32_t MIP_MEIP = 1 << 11;

    //  Exception cause codes (mcause/scause without interrupt bit)
    constexpr uint32_t CAUSE_MISALIGNED_FETCH = 0;
    constexpr uint32_t CAUSE_FETCH_ACCESS = 1;
    constexpr uint32_t CAUSE_ILLEGAL_INSTR = 2;
    constexpr uint32_t CAUSE_BREAKPOINT = 3;
    constexpr uint32_t CAUSE_MISALIGNED_LOAD = 4;
    constexpr uint32_t CAUSE_LOAD_ACCESS = 5;
    constexpr uint32_t CAUSE_MISALIGNED_STORE = 6;
    constexpr uint32_t CAUSE_STORE_ACCESS = 7;
    constexpr uint32_t CAUSE_ECALL_U = 8;
    constexpr uint32_t CAUSE_ECALL_S = 9;
    constexpr uint32_t CAUSE_ECALL_M = 11;
    constexpr uint32_t CAUSE_FETCH_PAGE_FAULT = 12;
    constexpr uint32_t CAUSE_LOAD_PAGE_FAULT = 13;
    constexpr uint32_t CAUSE_STORE_PAGE_FAULT = 15;

    //  Interrupt cause codes (mcause with bit 31 set)
    constexpr uint32_t INT_BIT = 0x80000000;
    constexpr uint32_t IRQ_S_SOFTWARE = INT_BIT | 1;
    constexpr uint32_t IRQ_M_SOFTWARE = INT_BIT | 3;
    constexpr uint32_t IRQ_S_TIMER = INT_BIT | 5;
    constexpr uint32_t IRQ_M_TIMER = INT_BIT | 7;
    constexpr uint32_t IRQ_S_EXTERNAL = INT_BIT | 9;
    constexpr uint32_t IRQ_M_EXTERNAL = INT_BIT | 11;

    //  Privilege levels
    constexpr uint8_t PRV_U = 0;
    constexpr uint8_t PRV_S = 1;
    constexpr uint8_t PRV_M = 3;

    //  SATP fields (Sv32)
    constexpr uint32_t SATP_MODE_BARE = 0;
    constexpr uint32_t SATP_MODE_SV32 = 1;
    constexpr uint32_t SATP_MODE_SHIFT = 31;
    constexpr uint32_t SATP_PPN_MASK = 0x003FFFFF;

    //  Page table entry bits
    constexpr uint32_t PTE_V = 1 << 0;
    constexpr uint32_t PTE_R = 1 << 1;
    constexpr uint32_t PTE_W = 1 << 2;
    constexpr uint32_t PTE_X = 1 << 3;
    constexpr uint32_t PTE_U = 1 << 4;
    constexpr uint32_t PTE_G = 1 << 5;
    constexpr uint32_t PTE_A = 1 << 6;
    constexpr uint32_t PTE_D = 1 << 7;
    constexpr uint32_t PTE_PPN_SHIFT = 10;

    //  Instruction field extraction
    inline uint32_t opcode(uint32_t instr) { return instr & 0x7F; }
    inline uint32_t rd(uint32_t instr) { return (instr >> 7) & 0x1F; }
    inline uint32_t funct3(uint32_t instr) { return (instr >> 12) & 0x7; }
    inline uint32_t rs1(uint32_t instr) { return (instr >> 15) & 0x1F; }
    inline uint32_t rs2(uint32_t instr) { return (instr >> 20) & 0x1F; }
    inline uint32_t funct7(uint32_t instr) { return (instr >> 25) & 0x7F; }
    inline uint32_t funct5(uint32_t instr) { return (instr >> 27) & 0x1F; }
    inline uint32_t funct12(uint32_t instr) { return (instr >> 20) & 0xFFF; }

    //  Immediate extraction (sign-extended to 32 bits)
    inline int32_t imm_i(uint32_t instr)
    {
        return static_cast<int32_t>(instr) >> 20;
    }

    inline int32_t imm_s(uint32_t instr)
    {
        return (static_cast<int32_t>(instr & 0xFE000000) >> 20) |
               ((instr >> 7) & 0x1F);
    }

    inline int32_t imm_b(uint32_t instr)
    {
        return (static_cast<int32_t>(instr & 0x80000000) >> 19) |
               ((instr & 0x80) << 4) |
               ((instr >> 20) & 0x7E0) |
               ((instr >> 7) & 0x1E);
    }

    inline int32_t imm_u(uint32_t instr)
    {
        return instr & 0xFFFFF000;
    }

    inline int32_t imm_j(uint32_t instr)
    {
        return (static_cast<int32_t>(instr & 0x80000000) >> 11) |
               (instr & 0xFF000) |
               ((instr >> 9) & 0x800) |
               ((instr >> 20) & 0x7FE);
    }

    // CSR immediate (zero-extended 5-bit)
    inline uint32_t csr_zimm(uint32_t instr) { return rs1(instr); }

    // MISA value for RV32IMAC
    constexpr uint32_t MISA_RV32 = (1 << 30); // MXL = 1 (32-bit)
    constexpr uint32_t MISA_I = 1 << ('I' - 'A');
    constexpr uint32_t MISA_M = 1 << ('M' - 'A');
    constexpr uint32_t MISA_A = 1 << ('A' - 'A');
    constexpr uint32_t MISA_C = 1 << ('C' - 'A');
    constexpr uint32_t MISA_S = 1 << ('S' - 'A');
    constexpr uint32_t MISA_U = 1 << ('U' - 'A');
    constexpr uint32_t MISA_VALUE = MISA_RV32 | MISA_I | MISA_M | MISA_A | MISA_C | MISA_S | MISA_U;

} // namespace rv32

#endif // GAMINGCPU_VP_RV32_DEFS_H
