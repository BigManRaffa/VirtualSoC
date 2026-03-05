#include "audio_out.h"
#include <cstring>

AudioOut::AudioOut(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &AudioOut::b_transport);
}

void AudioOut::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) ring_base_ = val;
        else val = ring_base_;
        break;
    case 0x04:
        if (is_write) ring_size_ = val;
        else val = ring_size_;
        break;
    case 0x08:
        if (is_write) rd_ptr_ = val;
        else val = rd_ptr_;
        break;
    case 0x0C:
        if (is_write) wr_ptr_ = val;
        else val = wr_ptr_;
        break;
    case 0x10:
        if (is_write) sample_rate_ = val;
        else val = sample_rate_;
        break;
    case 0x14:
        if (is_write) ctrl_ = val;
        else val = ctrl_;
        break;
    case 0x18:
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
