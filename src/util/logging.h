#ifndef GAMINGCPU_VP_LOGGING_H
#define GAMINGCPU_VP_LOGGING_H

#include <systemc>
#include <string>

#define VP_INFO(msg)  SC_REPORT_INFO("VP", msg)
#define VP_WARN(msg)  SC_REPORT_WARNING("VP", msg)
#define VP_ERROR(msg) SC_REPORT_ERROR("VP", msg)
#define VP_FATAL(msg) SC_REPORT_FATAL("VP", msg)

namespace logging {

void set_trace_enabled(bool enabled);
bool is_trace_enabled();

void trace_transaction(uint32_t addr, uint32_t data, bool is_write);

void trace_insn(uint32_t pc, uint32_t raw, uint32_t rd, int32_t val);

} // namespace logging

#endif // GAMINGCPU_VP_LOGGING_H
