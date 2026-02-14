#ifndef GAMINGCPU_VP_TRAP_H
#define GAMINGCPU_VP_TRAP_H

#include "execute.h"

namespace trap {

// Check for pending interrupts given current CPU state
// Returns the interrupt cause (with INT_BIT set) or 0 if none should be taken
uint32_t check_pending_interrupts(const CPUState& s);

// Enter trap handler! Handles M/S delegation via medeleg/mideleg, and saves CSRs (mepc/sepc, mcause/scause, etc.), 
// updates mstatus and privilege, and sets s.next_pc to the handler address (direct or vectored)
void take_trap(CPUState& s, uint32_t cause, uint32_t tval);

} // namespace trap

#endif // GAMINGCPU_VP_TRAP_H
