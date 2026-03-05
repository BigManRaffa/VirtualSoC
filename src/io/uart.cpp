#include "uart.h"
#include <cstring>

// 16550 register offsets (byte-addressed, but we get word offsets from bus)
constexpr uint32_t REG_RBR_THR = 0; // RX buffer (read) / TX holding (write)
constexpr uint32_t REG_IER     = 1; // Interrupt Enable
constexpr uint32_t REG_IIR_FCR = 2; // Interrupt ID (read) / FIFO Control (write)
constexpr uint32_t REG_LCR     = 3; // Line Control
constexpr uint32_t REG_MCR     = 4; // Modem Control
constexpr uint32_t REG_LSR     = 5; // Line Status
constexpr uint32_t REG_MSR     = 6; // Modem Status
constexpr uint32_t REG_SCR     = 7; // Scratch

// IER bits
constexpr uint8_t IER_RX_AVAIL = 0x01;
constexpr uint8_t IER_TX_EMPTY = 0x02;

// IIR values (active-low pending bit)
constexpr uint8_t IIR_NO_INT   = 0x01;
constexpr uint8_t IIR_TX_EMPTY = 0x02;
constexpr uint8_t IIR_RX_AVAIL = 0x04;
constexpr uint8_t IIR_FIFO_EN  = 0xC0; // FIFOs enabled indicator

// LSR bits
constexpr uint8_t LSR_DR   = 0x01; // Data Ready
constexpr uint8_t LSR_THRE = 0x20; // TX Holding Register Empty
constexpr uint8_t LSR_TEMT = 0x40; // Transmitter Empty

UART::UART(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &UART::b_transport);
}

void UART::push_rx(uint8_t byte) {
    if (rx_fifo_.size() < FIFO_SIZE)
        rx_fifo_.push(byte);
    update_irq();
}

void UART::update_irq() {
    bool fire = false;

    if ((ier_ & IER_RX_AVAIL) && !rx_fifo_.empty())
        fire = true;
    if ((ier_ & IER_TX_EMPTY) && tx_empty_)
        fire = true;

    if (on_irq)
        on_irq(fire);
}

void UART::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    uint32_t reg = addr; // bus gives us local offset, registers are at 0,1,2,...

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, len);

    switch (reg) {
    case REG_RBR_THR:
        if (is_write) {
            // TX: fire callback, transmit is instant in VP land
            tx_empty_ = false;
            if (on_tx)
                on_tx(static_cast<uint8_t>(val));
            tx_empty_ = true;
            update_irq();
        } else {
            // RX: pop from FIFO
            if (!rx_fifo_.empty()) {
                val = rx_fifo_.front();
                rx_fifo_.pop();
            } else {
                val = 0;
            }
            update_irq();
        }
        break;

    case REG_IER:
        if (is_write) {
            ier_ = val & 0x0F;
            update_irq();
        } else {
            val = ier_;
        }
        break;

    case REG_IIR_FCR:
        if (is_write) {
            // FCR write: we always have FIFOs enabled, accept and ignore
        } else {
            // IIR read: report highest priority pending interrupt
            if ((ier_ & IER_RX_AVAIL) && !rx_fifo_.empty())
                val = IIR_RX_AVAIL | IIR_FIFO_EN;
            else if ((ier_ & IER_TX_EMPTY) && tx_empty_)
                val = IIR_TX_EMPTY | IIR_FIFO_EN;
            else
                val = IIR_NO_INT | IIR_FIFO_EN;
        }
        break;

    case REG_LCR:
        if (is_write)
            lcr_ = val;
        else
            val = lcr_;
        break;

    case REG_MCR:
        if (is_write)
            mcr_ = val;
        else
            val = mcr_;
        break;

    case REG_LSR: {
        // read-only, synthesized from state
        uint8_t lsr = LSR_THRE | LSR_TEMT; // TX always ready in VP
        if (!rx_fifo_.empty())
            lsr |= LSR_DR;
        val = lsr;
        break;
    }

    case REG_MSR:
        val = 0; // no modem signals, we're not in 1995
        break;

    case REG_SCR:
        if (is_write)
            scr_ = val;
        else
            val = scr_;
        break;

    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, len);
}
