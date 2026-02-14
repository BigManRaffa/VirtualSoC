#include "csr.h"
#include "rv32_defs.h"

using namespace rv32;

CSRFile::CSRFile() {
    misa = MISA_VALUE;
    mstatus = 0;
}

// Privilege check!! CSR addr[9:8] encodes minimum privilege level
static bool priv_ok(uint16_t addr, uint8_t priv) {
    uint8_t required = (addr >> 8) & 0x3;
    return priv >= required;
}

static bool is_read_only(uint16_t addr) {
    return ((addr >> 10) & 0x3) == 0x3;
}

bool CSRFile::read(uint16_t addr, uint8_t priv, uint32_t& val) const {
    if (!priv_ok(addr, priv)) return false;

    switch (addr) {
    // Machine info (read-only constants)
    case CSR_MVENDORID: val = 0;          return true;
    case CSR_MARCHID:   val = 0;          return true;
    case CSR_MIMPID:    val = 0;          return true;
    case CSR_MHARTID:   val = 0;          return true;

    // Machine trap setup
    case CSR_MSTATUS:    val = mstatus;    return true;
    case CSR_MISA:       val = misa;       return true;
    case CSR_MEDELEG:    val = medeleg;    return true;
    case CSR_MIDELEG:    val = mideleg;    return true;
    case CSR_MIE:        val = mie;        return true;
    case CSR_MTVEC:      val = mtvec;      return true;
    case CSR_MCOUNTEREN: val = mcounteren; return true;

    // Machine trap handling
    case CSR_MSCRATCH: val = mscratch; return true;
    case CSR_MEPC:     val = mepc;     return true;
    case CSR_MCAUSE:   val = mcause;   return true;
    case CSR_MTVAL:    val = mtval;    return true;
    case CSR_MIP:      val = get_mip(); return true;

    // Machine counters
    case CSR_MCYCLE:    val = mcycle;    return true;
    case CSR_MCYCLEH:   val = mcycleh;   return true;
    case CSR_MINSTRET:  val = minstret;  return true;
    case CSR_MINSTRETH: val = minstreth; return true;

    // User counters (read-only shadows, gated by mcounteren/scounteren)
    case CSR_CYCLE:    val = mcycle;    return true;
    case CSR_CYCLEH:   val = mcycleh;   return true;
    case CSR_INSTRET:  val = minstret;  return true;
    case CSR_INSTRETH: val = minstreth; return true;
    case CSR_TIME:     val = mcycle;    return true; // // no RTC, alias to cycle counter
    case CSR_TIMEH:    val = mcycleh;   return true;

    // Supervisor trap setup
    case CSR_SSTATUS:    val = mstatus & SSTATUS_MASK; return true;
    case CSR_SIE:        val = mie & S_INT_MASK;       return true;
    case CSR_STVEC:      val = stvec;      return true;
    case CSR_SCOUNTEREN: val = scounteren; return true;

    // Supervisor trap handling
    case CSR_SSCRATCH: val = sscratch;             return true;
    case CSR_SEPC:     val = sepc;                 return true;
    case CSR_SCAUSE:   val = scause;               return true;
    case CSR_STVAL:    val = stval;                return true;
    case CSR_SIP:      val = get_mip() & S_INT_MASK; return true;
    case CSR_SATP:     val = satp;                 return true;

    default: return false;
    }
}

bool CSRFile::write(uint16_t addr, uint8_t priv, uint32_t val) {
    if (!priv_ok(addr, priv)) return false;
    if (is_read_only(addr))   return false;

    switch (addr) {
    // Machine trap setup
    case CSR_MSTATUS: {
        mstatus = (val & MSTATUS_WRITE_MASK);
        // Enforce MPP is a legal value (only M=3 or S=1 or U=0)
        uint32_t mpp = (mstatus >> 11) & 0x3;
        if (mpp == 2) mstatus = (mstatus & ~(3u << 11)); // illegal -> U
        return true;
    }
    case CSR_MISA:       return true; // writes ignored (fixed ISA!!!)
    case CSR_MEDELEG:    medeleg = val;    return true;
    case CSR_MIDELEG:    mideleg = val;    return true;
    case CSR_MIE:        mie = val;        return true;
    case CSR_MTVEC:      mtvec = val;      return true;
    case CSR_MCOUNTEREN: mcounteren = val; return true;

    // Machine trap handling
    case CSR_MSCRATCH: mscratch = val;       return true;
    case CSR_MEPC:     mepc = val & ~0x1u;   return true; // bit 0 always 0
    case CSR_MCAUSE:   mcause = val;         return true;
    case CSR_MTVAL:    mtval = val;          return true;
    case CSR_MIP:      sw_mip = val & (1 << 1); return true; // only SSIP writable

    // Machine counters
    case CSR_MCYCLE:    mcycle = val;    return true;
    case CSR_MCYCLEH:   mcycleh = val;   return true;
    case CSR_MINSTRET:  minstret = val;  return true;
    case CSR_MINSTRETH: minstreth = val; return true;

    // Supervisor trap setup
    case CSR_SSTATUS: {
        uint32_t new_bits = val & SSTATUS_MASK;
        mstatus = (mstatus & ~SSTATUS_MASK) | new_bits;
        return true;
    }
    case CSR_SIE: {
        uint32_t new_bits = val & S_INT_MASK;
        mie = (mie & ~S_INT_MASK) | new_bits;
        return true;
    }
    case CSR_STVEC:      stvec = val;      return true;
    case CSR_SCOUNTEREN: scounteren = val; return true;

    // Supervisor trap handling
    case CSR_SSCRATCH: sscratch = val;           return true;
    case CSR_SEPC:     sepc = val & ~0x1u;       return true;
    case CSR_SCAUSE:   scause = val;             return true;
    case CSR_STVAL:    stval = val;              return true;
    case CSR_SIP:      sw_mip = val & (1 << 1);  return true; // only SSIP
    case CSR_SATP:
        satp = val;
        if (on_satp_write) on_satp_write();
        return true;

    default: return false;
    }
}
