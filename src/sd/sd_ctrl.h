#ifndef GAMINGCPU_VP_SD_CTRL_H
#define GAMINGCPU_VP_SD_CTRL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>
#include <functional>
#include "sd_card_model.h"

// SD controller. Reads from backing image and DMA's blocks into VP memory
class SDCtrl : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<SDCtrl> tsock;
    tlm_utils::simple_initiator_socket<SDCtrl> isock;

    std::function<void(bool)> on_irq;

    SDCtrl(sc_core::sc_module_name name);
    SC_HAS_PROCESS(SDCtrl);

    void set_card(SDCardModel* card) { card_ = card; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void transfer_thread();

    // 0x00 command (CMD17=single block, CMD18=multi block)
    // 0x04 argument (block address)
    // 0x08 data address (DMA destination in VP memory)
    // 0x0C burst length (number of blocks for CMD18)
    // 0x10 status (bit0=busy, bit1=done, bit2=error, bit3=irq)
    // 0x14 ctrl (bit0=start)
    uint32_t cmd_ = 0;
    uint32_t arg_ = 0;
    uint32_t data_addr_ = 0;
    uint32_t burst_len_ = 1;
    uint32_t status_ = 0;

    sc_core::sc_event start_event_;
    SDCardModel* card_ = nullptr;

    static constexpr uint32_t CMD17 = 17;
    static constexpr uint32_t CMD18 = 18;
    static constexpr uint32_t STATUS_BUSY = 1;
    static constexpr uint32_t STATUS_DONE = 2;
    static constexpr uint32_t STATUS_ERROR = 4;
};

#endif // GAMINGCPU_VP_SD_CTRL_H
