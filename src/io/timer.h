#ifndef GAMINGCPU_VP_TIMER_H
#define GAMINGCPU_VP_TIMER_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>

// 64-bit system timer on MMIO bus. Like CLINT mtime but for S/U-mode code
class Timer : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<Timer> tsock;

    std::function<void(bool)> on_irq;

    Timer(sc_core::sc_module_name name, sc_core::sc_time tick_period);
    SC_HAS_PROCESS(Timer);

    uint64_t get_time() const { return time_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void tick_thread();
    void update_irq();

    sc_core::sc_time tick_period_;

    // 0x00 time_lo  0x04 time_hi  0x08 cmp_lo  0x0C cmp_hi  0x10 ctrl
    uint64_t time_ = 0;
    uint64_t cmp_ = 0xFFFFFFFFFFFFFFFFULL;
    uint32_t ctrl_ = 0;
};

#endif // GAMINGCPU_VP_TIMER_H
