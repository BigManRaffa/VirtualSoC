#ifndef GAMINGCPU_VP_ISS_H
#define GAMINGCPU_VP_ISS_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "execute.h"
#include "trap.h"

class ISS : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<ISS> isock;

    ISS(sc_core::sc_module_name name, uint32_t reset_pc);
    SC_HAS_PROCESS(ISS);

    CPUState state;
    bool stop_on_ebreak = false;
    uint64_t insn_count = 0;

private:
    void run();

    uint32_t bus_read(uint32_t addr, int bytes);
    void bus_write(uint32_t addr, uint32_t data, int bytes);

    void try_dmi(uint32_t addr);
    void invalidate_dmi(sc_dt::uint64 start, sc_dt::uint64 end);

    uint32_t reset_pc_;
    sc_core::sc_time clk_period_;

    bool dmi_valid_ = false;
    uint8_t* dmi_ptr_ = nullptr;
    uint64_t dmi_start_ = 0;
    uint64_t dmi_end_ = 0;
};

#endif // GAMINGCPU_VP_ISS_H
