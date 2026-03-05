#include "clint.h"
#include <cstring>

// Register offsets per SiFive CLINT spec (and our spec too)
// 0x0000  msip
// 0x4000  mtimecmp lo
// 0x4004  mtimecmp hi
// 0xBFF8  mtime lo
// 0xBFFC  mtime hi

CLINT::CLINT(sc_core::sc_module_name name, sc_core::sc_time tick_period)
    : sc_module(name)
    , tsock("tsock")
    , tick_period_(tick_period)
{
    tsock.register_b_transport(this, &CLINT::b_transport);
    SC_THREAD(tick_thread);
}

void CLINT::tick_thread() {
    while (true) {
        wait(tick_period_);
        mtime_++;
        update_timer_irq();
    }
}

void CLINT::update_timer_irq() {
    bool fire = (mtime_ >= mtimecmp_);
    if (on_timer_irq)
        on_timer_irq(fire);
}

void CLINT::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    // Only support 4-byte aligned accesses
    if (len != 4) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    switch (addr) {
    case 0x0000: // msip - bit 0 only
        if (is_write) {
            msip_ = val & 1;
            if (on_sw_irq)
                on_sw_irq(msip_ != 0);
        } else {
            val = msip_;
        }
        break;

    case 0x4000: // mtimecmp lo
        if (is_write) {
            mtimecmp_ = (mtimecmp_ & 0xFFFFFFFF00000000ULL) | val;
            update_timer_irq();
        } else {
            val = static_cast<uint32_t>(mtimecmp_);
        }
        break;

    case 0x4004: // mtimecmp hi
        if (is_write) {
            mtimecmp_ = (mtimecmp_ & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32);
            update_timer_irq();
        } else {
            val = static_cast<uint32_t>(mtimecmp_ >> 32);
        }
        break;

    case 0xBFF8: // mtime lo
        if (is_write) {
            mtime_ = (mtime_ & 0xFFFFFFFF00000000ULL) | val;
            update_timer_irq();
        } else {
            val = static_cast<uint32_t>(mtime_);
        }
        break;

    case 0xBFFC: // mtime hi
        if (is_write) {
            mtime_ = (mtime_ & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32);
            update_timer_irq();
        } else {
            val = static_cast<uint32_t>(mtime_ >> 32);
        }
        break;

    default:
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
