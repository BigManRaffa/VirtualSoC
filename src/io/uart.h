#ifndef GAMINGCPU_VP_UART_H
#define GAMINGCPU_VP_UART_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>
#include <queue>

// 16550-compatible UART. No baud rate nonsense, this is a VP not an FPGA
// TX writes go straight to a callback (putchar / TCP / whatever)
// RX comes from push_rx() (stdin poller or test harness)

class UART : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<UART> tsock;

    std::function<void(bool)> on_irq;
    std::function<void(uint8_t)> on_tx; // called when software writes THR

    UART(sc_core::sc_module_name name);
    SC_HAS_PROCESS(UART);

    // Shove a byte into the RX FIFO (called from stdin thread or test)
    void push_rx(uint8_t byte);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void update_irq();

    // 16550 registers
    std::queue<uint8_t> rx_fifo_;
    static constexpr size_t FIFO_SIZE = 16;

    uint8_t ier_ = 0; // Interrupt Enable Register
    uint8_t lcr_ = 0; // Line Control Register
    uint8_t mcr_ = 0; // Modem Control Register
    uint8_t scr_ = 0; // Scratch Register
    bool    tx_empty_ = true;
};

#endif // GAMINGCPU_VP_UART_H
