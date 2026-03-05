#ifndef GAMINGCPU_VP_GPIO_H
#define GAMINGCPU_VP_GPIO_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>

class GPIO : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<GPIO> tsock;

    std::function<void(bool)> on_irq;

    GPIO(sc_core::sc_module_name name);
    SC_HAS_PROCESS(GPIO);

    // Host-side (SDL2 keyboard etc) shoves new input pin state here
    void set_input(uint32_t pins);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void check_irq();

    // 0x00 direction (1=output)  0x04 output data
    // 0x08 input data  0x0C irq mask  0x10 irq status (w1c)
    uint32_t direction_ = 0;
    uint32_t output_ = 0;
    uint32_t input_ = 0;
    uint32_t irq_mask_ = 0;
    uint32_t irq_status_ = 0;
};

#endif // GAMINGCPU_VP_GPIO_H
