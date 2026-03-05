#include "sd_ctrl.h"
#include <cstring>

SDCtrl::SDCtrl(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
    , isock("isock")
{
    tsock.register_b_transport(this, &SDCtrl::b_transport);
    SC_THREAD(transfer_thread);
}

void SDCtrl::transfer_thread() {
    while (true) {
        wait(start_event_);

        status_ = STATUS_BUSY;
        uint32_t blocks = (cmd_ == CMD18) ? burst_len_ : 1;
        uint32_t block_addr = arg_;
        uint32_t dest = data_addr_;
        bool ok = true;

        for (uint32_t i = 0; i < blocks && ok; i++) {
            uint8_t buf[512];
            if (!card_ || !card_->read_block(block_addr + i, buf)) {
                ok = false;
                break;
            }

            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(dest);
            trans.set_data_ptr(buf);
            trans.set_data_length(512);
            trans.set_streaming_width(512);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            isock->b_transport(trans, delay);

            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
                ok = false;

            dest += 512;
        }

        status_ = ok ? STATUS_DONE : (STATUS_DONE | STATUS_ERROR);
        if (on_irq)
            on_irq(true);
    }
}

void SDCtrl::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) cmd_ = val;
        else val = cmd_;
        break;
    case 0x04:
        if (is_write) arg_ = val;
        else val = arg_;
        break;
    case 0x08:
        if (is_write) data_addr_ = val;
        else val = data_addr_;
        break;
    case 0x0C:
        if (is_write) burst_len_ = val;
        else val = burst_len_;
        break;
    case 0x10:
        if (is_write) { status_ = 0; if (on_irq) on_irq(false); }
        else val = status_;
        break;
    case 0x14:
        if (is_write && (val & 1))
            start_event_.notify(sc_core::SC_ZERO_TIME);
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
