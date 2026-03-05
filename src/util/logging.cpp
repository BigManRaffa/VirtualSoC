#include "logging.h"
#include <iostream>
#include <iomanip>

namespace logging {

static bool trace_enabled_ = false;

void set_trace_enabled(bool enabled) { trace_enabled_ = enabled; }
bool is_trace_enabled() { return trace_enabled_; }

void trace_transaction(uint32_t addr, uint32_t data, bool is_write) {
    if (!trace_enabled_) return;
    std::cout << "[TRC " << sc_core::sc_time_stamp() << "] "
              << (is_write ? "W" : "R") << " 0x"
              << std::hex << std::setfill('0') << std::setw(8) << addr
              << " = 0x" << std::setw(8) << data << std::dec << "\n";
}

void trace_insn(uint32_t pc, uint32_t raw, uint32_t rd, int32_t val) {
    if (!trace_enabled_) return;
    std::cout << "[INS " << sc_core::sc_time_stamp() << "] "
              << "0x" << std::hex << std::setfill('0') << std::setw(8) << pc
              << " " << std::setw(8) << raw
              << " x" << std::dec << rd << "=" << val << "\n";
}

} // namespace logging
