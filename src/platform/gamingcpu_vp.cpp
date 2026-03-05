#include "gamingcpu_vp.h"
#include "util/elf_loader.h"
#include <iostream>
#include <cstring>

GamingCPU_VP::GamingCPU_VP(sc_core::sc_module_name name, const std::string& elf_path)
    : sc_module(name)
    , cpu("cpu", cfg::RAM_BASE)
    , bus("bus")
    , ram("ram", cfg::RAM_BASE, cfg::RAM_SIZE)
    , bootrom("bootrom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE)
    , clint("clint", sc_core::sc_time(
          1.0e9 / cfg::CLINT_TICK_HZ, sc_core::SC_NS)) // 10MHz = 100ns
    , plic("plic")
    , uart("uart")
{
    // CPU -> Bus
    cpu.isock.bind(bus.tsock);

    // Bus -> Peripherals
    bus.isock.bind(bootrom.tsock);
    bus.map(cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);

    bus.isock.bind(ram.tsock);
    bus.map(cfg::RAM_BASE, cfg::RAM_SIZE);

    bus.isock.bind(clint.tsock);
    bus.map(cfg::CLINT_BASE, cfg::CLINT_SIZE);

    bus.isock.bind(plic.tsock);
    bus.map(cfg::PLIC_BASE, cfg::PLIC_SIZE);

    bus.isock.bind(uart.tsock);
    bus.map(cfg::UART_BASE, cfg::UART_SIZE);

    // CLINT -> ISS interrupt wiring
    clint.on_timer_irq = [this](bool v) { cpu.state.csr.set_mip_mtip(v); };
    clint.on_sw_irq    = [this](bool v) { cpu.state.csr.set_mip_msip(v); };

    // PLIC -> ISS external interrupt
    plic.on_external_irq = [this](bool v) { cpu.state.csr.set_mip_meip(v); };

    // UART -> PLIC interrupt source
    uart.on_irq = [this](bool v) { plic.set_pending(cfg::IRQ_UART, v); };

    // UART TX goes to stdout
    uart.on_tx = [](uint8_t c) { std::putchar(c); };

    // Load ELF if provided
    if (!elf_path.empty()) {
        auto result = load_elf(elf_path, [this](uint32_t paddr, const uint8_t* data, size_t len) {
            // Write through DMI pointer for speed
            if (paddr >= cfg::RAM_BASE && paddr + len <= cfg::RAM_BASE + cfg::RAM_SIZE) {
                std::memcpy(ram.data() + (paddr - cfg::RAM_BASE), data, len);
            } else if (paddr >= cfg::BOOTROM_BASE && paddr + len <= cfg::BOOTROM_BASE + cfg::BOOTROM_SIZE) {
                std::memcpy(bootrom.data() + (paddr - cfg::BOOTROM_BASE), data, len);
            }
        });
        std::cout << "[VP] ELF loaded: entry=0x" << std::hex << result.entry_point
                  << " segments=" << std::dec << result.segments_loaded << "\n";
    }
}
