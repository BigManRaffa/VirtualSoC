#include "gpio.h"
#include <cstring>

GPIO::GPIO(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &GPIO::b_transport);
}

void GPIO::set_input(uint32_t pins) {
    uint32_t old = input_;
    input_ = pins;
    uint32_t changed = old ^ pins;
    irq_status_ |= (changed & irq_mask_);
    check_irq();
}

void GPIO::check_irq() {
    if (on_irq)
        on_irq((irq_status_ & irq_mask_) != 0);
}

void GPIO::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) direction_ = val;
        else val = direction_;
        break;
    case 0x04:
        if (is_write) output_ = val;
        else val = output_;
        break;
    case 0x08:
        val = input_;
        break;
    case 0x0C:
        if (is_write) { irq_mask_ = val; check_irq(); }
        else val = irq_mask_;
        break;
    case 0x10:
        if (is_write) { irq_status_ &= ~val; check_irq(); } // write-1-to-clear
        else val = irq_status_;
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
