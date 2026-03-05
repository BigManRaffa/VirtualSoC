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

// Top-level SoC module. Owns everything, wires everything.
// Replaces rtl/subsys/soc_axi_top.sv + periph_axi_shell.sv
class GamingCPU_VP : public sc_core::sc_module
{
public:
    GamingCPU_VP(sc_core::sc_module_name name, const std::string& elf_path = "");
    SC_HAS_PROCESS(GamingCPU_VP);

    ISS     cpu;
    TLM_Bus bus;
    Memory  ram;
    BootROM bootrom;
    CLINT   clint;
    PLIC    plic;
    UART    uart;
};

#endif // GAMINGCPU_VP_PLATFORM_H
