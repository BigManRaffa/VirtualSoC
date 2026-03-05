#ifndef GAMINGCPU_VP_PLATFORM_H
#define GAMINGCPU_VP_PLATFORM_H

#include <systemc>
#include "platform_config.h"
#include "mem/memory.h"
#include "mem/bootrom.h"
#include "bus/tlm_bus.h"
#include "cpu/iss.h"
#include "irq/clint.h"
#include "irq/plic.h"
#include "io/uart.h"
#include "io/gpio.h"
#include "io/timer.h"
#include "io/spi.h"
#include "sd/sd_card_model.h"
#include "sd/sd_ctrl.h"
#include "dma/dma_engine.h"
#include "video/fb_ctrl.h"
#include "audio/audio_out.h"

// Replaces rtl/subsys/soc_axi_top.sv + periph_axi_shell.sv
class GamingCPU_VP : public sc_core::sc_module
{
public:
    GamingCPU_VP(sc_core::sc_module_name name, const std::string& elf_path = "",
                 const std::string& sd_image_path = "");
    SC_HAS_PROCESS(GamingCPU_VP);

    ISS       cpu;
    TLM_Bus   bus;
    Memory    ram;
    BootROM   bootrom;
    CLINT     clint;
    PLIC      plic;
    UART      uart;
    GPIO      gpio;
    Timer     timer;
    SPI       spi;
    SDCtrl    sd_ctrl;
    DMAEngine dma;
    FBCtrl    fb_ctrl;
    AudioOut  audio;

    SDCardModel sd_card;
};

#endif // GAMINGCPU_VP_PLATFORM_H
