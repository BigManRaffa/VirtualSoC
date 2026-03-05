#include "gamingcpu_vp.h"
#include "util/elf_loader.h"
#include <iostream>
#include <cstring>

GamingCPU_VP::GamingCPU_VP(sc_core::sc_module_name name,
                           const std::string& elf_path,
                           const std::string& sd_image_path)
    : sc_module(name)
    , cpu("cpu", cfg::RAM_BASE)
    , bus("bus")
    , ram("ram", cfg::RAM_BASE, cfg::RAM_SIZE)
    , bootrom("bootrom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE)
    , clint("clint", sc_core::sc_time(1.0e9 / cfg::CLINT_TICK_HZ, sc_core::SC_NS))
    , plic("plic")
    , uart("uart")
    , gpio("gpio")
    , timer("timer", sc_core::sc_time(1.0e9 / cfg::CLINT_TICK_HZ, sc_core::SC_NS))
    , spi("spi")
    , sd_ctrl("sd_ctrl")
    , dma("dma")
    , fb_ctrl("fb_ctrl")
    , audio("audio")
{
    // Masters -> Bus
    cpu.isock.bind(bus.tsock);
    dma.isock.bind(bus.tsock);
    sd_ctrl.isock.bind(bus.tsock);

    // Bus -> Memory
    bus.isock.bind(bootrom.tsock);
    bus.map(cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);

    bus.isock.bind(ram.tsock);
    bus.map(cfg::RAM_BASE, cfg::RAM_SIZE);

    // Bus -> Interrupt controllers
    bus.isock.bind(clint.tsock);
    bus.map(cfg::CLINT_BASE, cfg::CLINT_SIZE);

    bus.isock.bind(plic.tsock);
    bus.map(cfg::PLIC_BASE, cfg::PLIC_SIZE);

    // Bus -> Peripherals
    bus.isock.bind(uart.tsock);
    bus.map(cfg::UART_BASE, cfg::UART_SIZE);

    bus.isock.bind(gpio.tsock);
    bus.map(cfg::GPIO_BASE, cfg::GPIO_SIZE);

    bus.isock.bind(timer.tsock);
    bus.map(cfg::TIMER_BASE, cfg::TIMER_SIZE);

    bus.isock.bind(spi.tsock);
    bus.map(cfg::SPI_BASE, cfg::SPI_SIZE);

    bus.isock.bind(sd_ctrl.tsock);
    bus.map(cfg::SD_BASE, cfg::SD_SIZE);

    bus.isock.bind(dma.tsock);
    bus.map(cfg::DMA_BASE, cfg::DMA_SIZE);

    bus.isock.bind(fb_ctrl.tsock);
    bus.map(cfg::VIDEO_BASE, cfg::VIDEO_SIZE);

    bus.isock.bind(audio.tsock);
    bus.map(cfg::AUDIO_BASE, cfg::AUDIO_SIZE);

    // CLINT -> ISS
    clint.on_timer_irq = [this](bool v) {
        cpu.state.csr.set_mip_mtip(v);
        cpu.notify_wfi();
    };
    clint.on_sw_irq = [this](bool v) {
        cpu.state.csr.set_mip_msip(v);
        cpu.notify_wfi();
    };

    // PLIC -> ISS
    plic.on_external_irq = [this](bool v) {
        cpu.state.csr.set_mip_meip(v);
        cpu.notify_wfi();
    };

    // Peripheral IRQs -> PLIC (spec Table 3)
    uart.on_irq    = [this](bool v) { plic.set_pending(cfg::IRQ_UART, v); };
    gpio.on_irq    = [this](bool v) { plic.set_pending(cfg::IRQ_GPIO, v); };
    timer.on_irq   = [this](bool v) { plic.set_pending(cfg::IRQ_TIMER, v); };
    sd_ctrl.on_irq = [this](bool v) { plic.set_pending(cfg::IRQ_SD, v); };
    dma.on_irq     = [this](bool v) { plic.set_pending(cfg::IRQ_DMA, v); };
    fb_ctrl.on_irq = [this](bool v) { plic.set_pending(cfg::IRQ_VIDEO, v); };
    audio.on_irq   = [this](bool v) { plic.set_pending(cfg::IRQ_AUDIO, v); };

    uart.on_tx = [](uint8_t c) { std::putchar(c); };

    // SD card
    if (!sd_image_path.empty() && sd_card.open(sd_image_path))
        sd_ctrl.set_card(&sd_card);

    // Load ELF
    if (!elf_path.empty()) {
        auto result = load_elf(elf_path, [this](uint32_t paddr, const uint8_t* data, size_t len) {
            if (paddr >= cfg::RAM_BASE && paddr + len <= cfg::RAM_BASE + cfg::RAM_SIZE)
                std::memcpy(ram.data() + (paddr - cfg::RAM_BASE), data, len);
            else if (paddr >= cfg::BOOTROM_BASE && paddr + len <= cfg::BOOTROM_BASE + cfg::BOOTROM_SIZE)
                std::memcpy(bootrom.data() + (paddr - cfg::BOOTROM_BASE), data, len);
        });
        cpu.state.pc = result.entry_point;
        std::cout << "[VP] ELF loaded: entry=0x" << std::hex << result.entry_point
                  << " segments=" << std::dec << result.segments_loaded << "\n";
    }
}
