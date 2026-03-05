#ifndef GAMINGCPU_VP_CLINT_H
#define GAMINGCPU_VP_CLINT_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>

class CLINT : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<CLINT> tsock;

    // Callback to poke mip.MTIP and mip.MSIP on the ISS
    std::function<void(bool)> on_timer_irq;
    std::function<void(bool)> on_sw_irq;

    CLINT(sc_core::sc_module_name name, sc_core::sc_time tick_period);
    SC_HAS_PROCESS(CLINT);

    uint64_t get_mtime() const { return mtime_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void tick_thread();
    void update_timer_irq();

    sc_core::sc_time tick_period_;

    uint64_t mtime_ = 0;
    uint64_t mtimecmp_ = 0xFFFFFFFFFFFFFFFFULL; // max so no spurious IRQ at boot
    uint32_t msip_ = 0;
};

#endif // GAMINGCPU_VP_CLINT_H
