#ifndef GAMINGCPU_VP_DMA_ENGINE_H
#define GAMINGCPU_VP_DMA_ENGINE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>
#include <functional>

class DMAEngine : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<DMAEngine> tsock;
    tlm_utils::simple_initiator_socket<DMAEngine> isock;

    std::function<void(bool)> on_irq;

    DMAEngine(sc_core::sc_module_name name);
    SC_HAS_PROCESS(DMAEngine);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void dma_thread();

    // 0x00 SRC_ADDR  0x04 DST_ADDR  0x08 BYTE_COUNT
    // 0x0C CTRL (bit0=start, bit1=irq enable)
    // 0x10 STATUS (bit0=busy, bit1=done, bit2=error)
    uint32_t src_addr_ = 0;
    uint32_t dst_addr_ = 0;
    uint32_t byte_count_ = 0;
    uint32_t ctrl_ = 0;
    uint32_t status_ = 0;

    sc_core::sc_event start_event_;
    static constexpr uint32_t BURST_SIZE = 256;
};

#endif // GAMINGCPU_VP_DMA_ENGINE_H
