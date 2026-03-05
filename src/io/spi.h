#ifndef GAMINGCPU_VP_SPI_H
#define GAMINGCPU_VP_SPI_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>

// Minimal SPI master. Clock/polarity/phase regs accepted but ignored, this is a VP not an FPGA
class SPI : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<SPI> tsock;

    std::function<uint8_t(uint8_t)> on_transfer;

    SPI(sc_core::sc_module_name name);
    SC_HAS_PROCESS(SPI);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // 0x00 TX data (write triggers transfer)
    // 0x04 RX data (read gets last received byte)
    // 0x08 status (bit 0 = ready, always 1 in VP)
    // 0x0C clock divider (accepted, ignored)
    // 0x10 config (CPOL/CPHA, accepted, ignored)
    uint8_t rx_data_ = 0xFF;
    uint32_t clk_div_ = 0;
    uint32_t config_ = 0;
};

#endif // GAMINGCPU_VP_SPI_H
