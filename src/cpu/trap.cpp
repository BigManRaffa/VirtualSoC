#include "trap.h"
#include "rv32_defs.h"

using namespace rv32;

uint32_t trap::check_pending_interrupts(const CPUState& s) {
    uint32_t pending = s.csr.get_mip() & s.csr.mie;
    if (pending == 0) return 0;

    uint32_t m_pending = pending & ~s.csr.mideleg;
    uint32_t s_pending = pending & s.csr.mideleg;

    bool m_enabled = (s.priv < PRV_M) ||
                     (s.priv == PRV_M && (s.csr.mstatus & MSTATUS_MIE));

    bool s_enabled = (s.priv < PRV_S) ||
                     (s.priv == PRV_S && (s.csr.mstatus & MSTATUS_SIE));
    if (s.priv == PRV_M) s_enabled = false;

    uint32_t active = (m_enabled ? m_pending : 0) | (s_enabled ? s_pending : 0);
    if (active == 0) return 0;

    // MEI > MSI > MTI > SEI > SSI > STI
    static const int priority[] = {11, 3, 7, 9, 1, 5};
    for (int bit : priority) {
        if (active & (1u << bit))
            return INT_BIT | bit;
    }

    return 0;
}

void trap::take_trap(CPUState& s, uint32_t cause, uint32_t tval) {
    bool is_interrupt = (cause & INT_BIT) != 0;
    uint32_t cause_code = cause & 0x7FFFFFFF;

    // Delegate to S-mode if: priv <= S and the corresponding deleg bit is set
    bool delegate = false;
    if (s.priv <= PRV_S) {
        uint32_t deleg = is_interrupt ? s.csr.mideleg : s.csr.medeleg;
        delegate = (deleg >> cause_code) & 1;
    }

    if (delegate) {
        s.csr.sepc = s.pc & ~0x1u;
        s.csr.scause = cause;
        s.csr.stval = tval;

        bool sie = (s.csr.mstatus & MSTATUS_SIE) != 0;
        s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_SPIE) | (sie ? MSTATUS_SPIE : 0);
        s.csr.mstatus &= ~MSTATUS_SIE;

        s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_SPP) |
                         ((s.priv == PRV_S) ? MSTATUS_SPP : 0);

        s.priv = PRV_S;

        uint32_t base = s.csr.stvec & ~0x3u;
        uint32_t mode = s.csr.stvec & 0x3;
        s.next_pc = (mode == 1 && is_interrupt) ? base + 4 * cause_code : base;
    } else {
        s.csr.mepc = s.pc & ~0x1u;
        s.csr.mcause = cause;
        s.csr.mtval = tval;

        bool mie = (s.csr.mstatus & MSTATUS_MIE) != 0;
        s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_MPIE) | (mie ? MSTATUS_MPIE : 0);
        s.csr.mstatus &= ~MSTATUS_MIE;

        s.csr.mstatus = (s.csr.mstatus & ~MSTATUS_MPP_MASK) |
                         (static_cast<uint32_t>(s.priv) << MSTATUS_MPP_SHIFT);

        s.priv = PRV_M;

        uint32_t base = s.csr.mtvec & ~0x3u;
        uint32_t mode = s.csr.mtvec & 0x3;
        s.next_pc = (mode == 1 && is_interrupt) ? base + 4 * cause_code : base;
    }
}
