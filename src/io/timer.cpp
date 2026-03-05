#include "timer.h"
#include <cstring>

Timer::Timer(sc_core::sc_module_name name, sc_core::sc_time tick_period)
    : sc_module(name)
    , tsock("tsock")
    , tick_period_(tick_period)
{
    tsock.register_b_transport(this, &Timer::b_transport);
    SC_THREAD(tick_thread);
}

void Timer::tick_thread() {
    while (true) {
        wait(tick_period_);
        time_++;
        update_irq();
    }
}

void Timer::update_irq() {
    bool fire = (ctrl_ & 1) && (time_ >= cmp_);
    if (on_irq)
        on_irq(fire);
}

void Timer::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) { time_ = (time_ & 0xFFFFFFFF00000000ULL) | val; update_irq(); }
        else val = static_cast<uint32_t>(time_);
        break;
    case 0x04:
        if (is_write) { time_ = (time_ & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32); update_irq(); }
        else val = static_cast<uint32_t>(time_ >> 32);
        break;
    case 0x08:
        if (is_write) { cmp_ = (cmp_ & 0xFFFFFFFF00000000ULL) | val; update_irq(); }
        else val = static_cast<uint32_t>(cmp_);
        break;
    case 0x0C:
        if (is_write) { cmp_ = (cmp_ & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32); update_irq(); }
        else val = static_cast<uint32_t>(cmp_ >> 32);
        break;
    case 0x10:
        if (is_write) { ctrl_ = val; update_irq(); }
        else val = ctrl_;
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
