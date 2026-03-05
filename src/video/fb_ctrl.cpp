#include "fb_ctrl.h"
#include <cstring>

FBCtrl::FBCtrl(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &FBCtrl::b_transport);
}

void FBCtrl::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) fb0_addr_ = val;
        else val = fb0_addr_;
        break;
    case 0x04:
        if (is_write) fb1_addr_ = val;
        else val = fb1_addr_;
        break;
    case 0x08:
        if (is_write) stride_ = val;
        else val = stride_;
        break;
    case 0x0C:
        if (is_write) pal_addr_ = val;
        else val = pal_addr_;
        break;
    case 0x10:
        if (is_write) {
            active_buf_ ^= 1;
            vsync_pending_ = 1;
            uint32_t fb_addr = active_buf_ ? fb1_addr_ : fb0_addr_;
            if (on_vsync)
                on_vsync(fb_addr, pal_addr_, stride_);
            if (on_irq)
                on_irq(true);
        }
        break;
    case 0x14:
        if (is_write) { vsync_pending_ = 0; if (on_irq) on_irq(false); }
        else val = (active_buf_) | (vsync_pending_ << 1);
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
