#ifndef GAMINGCPU_VP_EXECUTE_H
#define GAMINGCPU_VP_EXECUTE_H

#include <cstdint>
#include <functional>
#include "decode.h"
#include "csr.h"
#include "rv32a.h"

struct MemIf {
    std::function<uint32_t(uint32_t addr, int bytes)> read;
    std::function<void(uint32_t addr, uint32_t data, int bytes)> write;
};

struct CPUState {
    int32_t  regs[32] = {};
    uint32_t pc       = 0;
    uint32_t next_pc  = 0;
    uint8_t  priv     = 3; // M-mode

    CSRFile  csr;
    Reservation lr_sc;

    MemIf    mem;

    int32_t  get_reg(uint32_t i) const { return (i == 0) ? 0 : regs[i]; }
    uint32_t get_regu(uint32_t i) const { return static_cast<uint32_t>(get_reg(i)); }
    void     set_reg(uint32_t i, int32_t v) { if (i != 0) regs[i] = v; }
};

struct ExecResult {
    bool     exception = false;
    uint32_t cause     = 0;
    uint32_t tval      = 0;
};

ExecResult execute(CPUState& s, const DecodedInstr& d);

#endif // GAMINGCPU_VP_EXECUTE_H
