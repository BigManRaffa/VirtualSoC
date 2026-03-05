#include "plic.h"
#include <cstring>

PLIC::PLIC(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
{
    tsock.register_b_transport(this, &PLIC::b_transport);
}

void PLIC::set_pending(uint32_t source_id, bool pending) {
    if (source_id == 0 || source_id >= NUM_SOURCES)
        return;

    if (pending)
        pending_ |= (1u << source_id);
    else
        pending_ &= ~(1u << source_id);

    evaluate_irq();
}

void PLIC::evaluate_irq() {
    // Find highest-priority pending+enabled interrupt that beats the threshold
    // Source 0 is reserved, skip it. Highest priority wins, lowest ID breaks ties
    uint32_t best_prio = 0;
    uint32_t actionable = pending_ & enabled_ & ~claimed_;

    for (uint32_t i = 1; i < NUM_SOURCES; i++) {
        if ((actionable & (1u << i)) && priority_[i] > threshold_ && priority_[i] > best_prio)
            best_prio = priority_[i];
    }

    if (on_external_irq)
        on_external_irq(best_prio > 0);
}

uint32_t PLIC::claim_best() {
    uint32_t best_id = 0;
    uint32_t best_prio = 0;
    uint32_t actionable = pending_ & enabled_ & ~claimed_;

    for (uint32_t i = 1; i < NUM_SOURCES; i++) {
        if ((actionable & (1u << i)) && priority_[i] > threshold_ && priority_[i] > best_prio) {
            best_prio = priority_[i];
            best_id = i;
        }
    }

    if (best_id) {
        // Mark as claimed, clear pending
        claimed_ |= (1u << best_id);
        pending_ &= ~(1u << best_id);
        evaluate_irq();
    }

    return best_id;
}

void PLIC::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    if (len != 4) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    uint32_t val = 0;
    if (is_write)
        std::memcpy(&val, ptr, 4);

    // Priority registers: 0x000000 + source_id * 4
    if (addr < NUM_SOURCES * 4) {
        uint32_t src = addr / 4;
        if (is_write) {
            if (src != 0) // source 0 priority is hardwired to 0. nice try
                priority_[src] = val & 0x7; // 3-bit priority (0-7)
            evaluate_irq();
        } else {
            val = priority_[src];
        }
    }
    // Pending bits: 0x001000
    else if (addr == 0x1000) {
        if (is_write) {
            // pending is read-only from software side, peripherals set it
            // but we allow it for testing because we're cool like that
            pending_ = val;
            evaluate_irq();
        } else {
            val = pending_;
        }
    }
    // Enable bits: 0x002000
    else if (addr == 0x2000) {
        if (is_write) {
            enabled_ = val;
            evaluate_irq();
        } else {
            val = enabled_;
        }
    }
    // Threshold: 0x200000
    else if (addr == 0x200000) {
        if (is_write) {
            threshold_ = val & 0x7;
            evaluate_irq();
        } else {
            val = threshold_;
        }
    }
    // Claim/Complete: 0x200004
    else if (addr == 0x200004) {
        if (is_write) {
            // Complete: release the claimed source
            if (val < NUM_SOURCES)
                claimed_ &= ~(1u << val);
            evaluate_irq();
        } else {
            val = claim_best();
        }
    }
    else {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (!is_write)
        std::memcpy(ptr, &val, 4);
}
