#ifndef GAMINGCPU_VP_FB_CTRL_H
#define GAMINGCPU_VP_FB_CTRL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <functional>
#include "platform/platform_config.h"

// Double-buffered 320x200 indexed-color framebuffer controller
class FBCtrl : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<FBCtrl> tsock;

    std::function<void(bool)> on_irq;

    std::function<void(uint32_t fb_addr, uint32_t pal_addr, uint32_t stride)> on_vsync;

    FBCtrl(sc_core::sc_module_name name);
    SC_HAS_PROCESS(FBCtrl);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // 0x00 FB0_ADDR  0x04 FB1_ADDR  0x08 STRIDE
    // 0x0C PAL_ADDR  0x10 VSYNC_CTRL (write triggers swap)
    // 0x14 VSYNC_STATUS (bit0=active buffer, bit1=vsync pending)
    uint32_t fb0_addr_ = cfg::FB0_DEFAULT;
    uint32_t fb1_addr_ = cfg::FB1_DEFAULT;
    uint32_t stride_ = 320;
    uint32_t pal_addr_ = cfg::PALETTE_DEFAULT;
    uint32_t active_buf_ = 0;
    uint32_t vsync_pending_ = 0;
};

#endif // GAMINGCPU_VP_FB_CTRL_H
