#ifndef GAMINGCPU_VP_PLIC_H
#define GAMINGCPU_VP_PLIC_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>
#include "platform/platform_config.h"

// SiFive-style PLIC register map (single context for our single hart):
// 0x000000  source 0 priority (reserved, always 0)
// 0x000004  source 1 priority
// ...
// 0x001000  pending bits [31:0] (bit N = source N)
// 0x002000  enable bits [31:0] for context 0
// 0x200000  priority threshold for context 0
// 0x200004  claim/complete for context 0

class PLIC : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<PLIC> tsock;

    std::function<void(bool)> on_external_irq;

    PLIC(sc_core::sc_module_name name);
    SC_HAS_PROCESS(PLIC);

    // Peripherals call this to assert/deassert their interrupt line
    void set_pending(uint32_t source_id, bool pending);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void evaluate_irq();
    uint32_t claim_best();

    static constexpr uint32_t NUM_SOURCES = cfg::IRQ_NUM_SOURCES;

    uint32_t priority_[NUM_SOURCES] = {};
    uint32_t pending_ = 0;
    uint32_t enabled_ = 0;
    uint32_t threshold_ = 0;
    uint32_t claimed_ = 0; // sources currently in-service
};

#endif // GAMINGCPU_VP_PLIC_H
