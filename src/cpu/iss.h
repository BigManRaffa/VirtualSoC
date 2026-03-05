#ifndef GAMINGCPU_VP_ISS_H
#define GAMINGCPU_VP_ISS_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "execute.h"
#include "trap.h"
#include "mmu.h"

class ISS : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<ISS> isock;

    ISS(sc_core::sc_module_name name, uint32_t reset_pc);
    SC_HAS_PROCESS(ISS);

    CPUState state;
    MMU mmu;

    bool stop_on_ebreak = false;
    uint64_t insn_count = 0;

    void notify_wfi() { wfi_event_.notify(); }

    // GDB debug control
    void halt();
    void resume();
    void step();
    sc_core::sc_event halted_event;

    // Physical bus access (GDB uses these for memory read/write)
    uint32_t bus_read(uint32_t paddr, int bytes);
    void bus_write(uint32_t paddr, uint32_t data, int bytes);

private:
    void run();

    bool mmu_active_fetch() const;
    bool mmu_active_data() const;
    uint8_t effective_data_priv() const;

    void try_dmi(uint32_t addr);
    void invalidate_dmi(sc_dt::uint64 start, sc_dt::uint64 end);

    uint32_t reset_pc_;
    sc_core::sc_time clk_period_;
    sc_core::sc_event wfi_event_;
    sc_core::sc_event resume_event_;

    bool halted_ = false;
    bool single_step_ = false;

    bool mem_fault_ = false;
    uint32_t mem_fault_cause_ = 0;
    uint32_t mem_fault_vaddr_ = 0;

    bool dmi_valid_ = false;
    uint8_t* dmi_ptr_ = nullptr;
    uint64_t dmi_start_ = 0;
    uint64_t dmi_end_ = 0;
};

#endif // GAMINGCPU_VP_ISS_H
