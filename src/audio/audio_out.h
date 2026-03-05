#ifndef GAMINGCPU_VP_AUDIO_OUT_H
#define GAMINGCPU_VP_AUDIO_OUT_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>
#include "platform/platform_config.h"

// Ring buffer PCM audio output
class AudioOut : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<AudioOut> tsock;

    std::function<void(bool)> on_irq;

    AudioOut(sc_core::sc_module_name name);
    SC_HAS_PROCESS(AudioOut);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // 0x00 RING_BASE  0x04 RING_SIZE  0x08 RD_PTR  0x0C WR_PTR
    // 0x10 SAMPLE_RATE  0x14 CTRL (bit0=enable)  0x18 STATUS (bit0=underrun)
    uint32_t ring_base_ = cfg::AUDIO_RING_DEFAULT;
    uint32_t ring_size_ = cfg::AUDIO_RING_SIZE_DEFAULT;
    uint32_t rd_ptr_ = 0;
    uint32_t wr_ptr_ = 0;
    uint32_t sample_rate_ = 11025;
    uint32_t ctrl_ = 0;
    uint32_t status_ = 0;
};

#endif // GAMINGCPU_VP_AUDIO_OUT_H
