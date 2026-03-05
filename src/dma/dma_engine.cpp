#include "dma_engine.h"
#include <cstring>
#include <algorithm>

DMAEngine::DMAEngine(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
    , isock("isock")
{
    tsock.register_b_transport(this, &DMAEngine::b_transport);
    SC_THREAD(dma_thread);
}

void DMAEngine::dma_thread() {
    while (true) {
        wait(start_event_);

        status_ = 1; // busy
        uint32_t remaining = byte_count_;
        uint32_t src = src_addr_;
        uint32_t dst = dst_addr_;
        bool ok = true;

        while (remaining > 0 && ok) {
            uint32_t chunk = std::min(remaining, BURST_SIZE);
            uint8_t buf[BURST_SIZE];

            tlm::tlm_generic_payload rtrans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            rtrans.set_command(tlm::TLM_READ_COMMAND);
            rtrans.set_address(src);
            rtrans.set_data_ptr(buf);
            rtrans.set_data_length(chunk);
            rtrans.set_streaming_width(chunk);
            rtrans.set_byte_enable_ptr(nullptr);
            rtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            isock->b_transport(rtrans, delay);
            if (rtrans.get_response_status() != tlm::TLM_OK_RESPONSE) { ok = false; break; }

            tlm::tlm_generic_payload wtrans;
            delay = sc_core::SC_ZERO_TIME;
            wtrans.set_command(tlm::TLM_WRITE_COMMAND);
            wtrans.set_address(dst);
            wtrans.set_data_ptr(buf);
            wtrans.set_data_length(chunk);
            wtrans.set_streaming_width(chunk);
            wtrans.set_byte_enable_ptr(nullptr);
            wtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            isock->b_transport(wtrans, delay);
            if (wtrans.get_response_status() != tlm::TLM_OK_RESPONSE) { ok = false; break; }

            src += chunk;
            dst += chunk;
            remaining -= chunk;
        }

        status_ = ok ? 2 : 6; // done or done+error
        if ((ctrl_ & 2) && on_irq)
            on_irq(true);
    }
}

void DMAEngine::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) src_addr_ = val;
        else val = src_addr_;
        break;
    case 0x04:
        if (is_write) dst_addr_ = val;
        else val = dst_addr_;
        break;
    case 0x08:
        if (is_write) byte_count_ = val;
        else val = byte_count_;
        break;
    case 0x0C:
        if (is_write) {
            ctrl_ = val;
            if (val & 1) start_event_.notify(sc_core::SC_ZERO_TIME);
        } else val = ctrl_;
        break;
    case 0x10:
        if (is_write) { status_ = 0; if (on_irq) on_irq(false); }
        else val = status_;
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
