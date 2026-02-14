#ifndef GAMINGCPU_VP_DECODE_H
#define GAMINGCPU_VP_DECODE_H

#include <cstdint>

// Instruction types for execute dispatch
enum class InstrType
{
    // Upper Immediate / Jump
    LUI,
    AUIPC,
    JAL,
    JALR,

    // Branch
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,

    // Load
    LB,
    LH,
    LW,
    LBU,
    LHU,

    // Store
    SB,
    SH,
    SW,

    // Immediate ALU
    ADDI,
    SLTI,
    SLTIU,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,

    // Register ALU
    ADD,
    SUB,
    SLL,
    SLT,
    SLTU,
    XOR,
    SRL,
    SRA,
    OR,
    AND,

    // Multiply / Divide
    MUL,
    MULH,
    MULHSU,
    MULHU,
    DIV,
    DIVU,
    REM,
    REMU,

    // A Extension, Atomics
    LR_W,
    SC_W,
    AMOSWAP_W,
    AMOADD_W,
    AMOXOR_W,
    AMOAND_W,
    AMOOR_W,
    AMOMIN_W,
    AMOMAX_W,
    AMOMINU_W,
    AMOMAXU_W,

    // System, Trap
    ECALL, EBREAK, MRET, SRET, URET, WFI, SFENCE_VMA,

    // System, CSR
    CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, SRRCI,

    // System, Fence
    FENCE, FENCEI,

    // Invalid
    ILLEGAL
};

struct DecodedInstr
{
    InstrType type = InstrType::ILLEGAL;
    uint32_t rd = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    int32_t imm = 0;
    uint32_t csr = 0;        // CSR address for CSR instructions
    uint32_t raw = 0;        // Original instruction word
    bool compressed = false; // True if was 16-bit RVC

    uint32_t instr_len() const { return compressed ? 2 : 4; }
};

// Stateless decoder — handles RV32IMAC including compressed expansion
DecodedInstr decode(uint32_t instr);

// Expand a 16-bit compressed instruction to its 32-bit equivalent.
// Returns 0 (illegal) if the compressed instruction has no mapping.
uint32_t expand_compressed(uint16_t cinstr);

#endif // GAMINGCPU_VP_DECODE_H