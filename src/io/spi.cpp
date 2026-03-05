#include "spi.h"
#include <cstring>

SPI::SPI(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &SPI::b_transport);
}

void SPI::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x00:
        if (is_write) {
            if (on_transfer)
                rx_data_ = on_transfer(static_cast<uint8_t>(val));
            else
                rx_data_ = 0xFF;
        }
        break;
    case 0x04:
        val = rx_data_;
        break;
    case 0x08:
        val = 1;
        break;
    case 0x0C:
        if (is_write) clk_div_ = val;
        else val = clk_div_;
        break;
    case 0x10:
        if (is_write) config_ = val;
        else val = config_;
        break;
    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
