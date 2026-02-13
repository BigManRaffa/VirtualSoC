#ifndef GAMINGCPU_VP_PLATFORM_CONFIG_H
#define GAMINGCPU_VP_PLATFORM_CONFIG_H

#include <cstdint>

// Memory map constants
// Single source of truth for all addresses in the VP
// Generated from specs/memory_map.yaml
namespace cfg
{

    // BootROM: 64 KB
    constexpr uint32_t BOOTROM_BASE = 0x00000000;
    constexpr uint32_t BOOTROM_SIZE = 0x00010000; // 64 KB

    // On-chip SRAM: 64 KB
    constexpr uint32_t SRAM_BASE = 0x01000000;
    constexpr uint32_t SRAM_SIZE = 0x00010000; // 64 KB

    // CLINT: 64 KB
    constexpr uint32_t CLINT_BASE = 0x02000000;
    constexpr uint32_t CLINT_SIZE = 0x00010000; // 64 KB

    // PLIC: 64 MB
    constexpr uint32_t PLIC_BASE = 0x0C000000;
    constexpr uint32_t PLIC_SIZE = 0x04000000; // 64 MB

    // UART: 4 KB
    constexpr uint32_t UART_BASE = 0x10000000;
    constexpr uint32_t UART_SIZE = 0x00001000; // 4 KB

    // GPIO: 4 KB
    constexpr uint32_t GPIO_BASE = 0x10001000;
    constexpr uint32_t GPIO_SIZE = 0x00001000; // 4 KB

    // Timer64: 4 KB
    constexpr uint32_t TIMER_BASE = 0x10002000;
    constexpr uint32_t TIMER_SIZE = 0x00001000; // 4 KB

    // SPI Master: 4 KB
    constexpr uint32_t SPI_BASE = 0x10003000;
    constexpr uint32_t SPI_SIZE = 0x00001000; // 4 KB

    // SD Controller: 4 KB
    constexpr uint32_t SD_BASE = 0x10004000;
    constexpr uint32_t SD_SIZE = 0x00001000; // 4 KB

    // DMA Engine: 4 KB
    constexpr uint32_t DMA_BASE = 0x10005000;
    constexpr uint32_t DMA_SIZE = 0x00001000; // 4 KB

    // Video (Framebuffer Ctrl): 4 KB
    constexpr uint32_t VIDEO_BASE = 0x10006000;
    constexpr uint32_t VIDEO_SIZE = 0x00001000; // 4 KB

    // Audio (I2S/PWM): 4 KB
    constexpr uint32_t AUDIO_BASE = 0x10007000;
    constexpr uint32_t AUDIO_SIZE = 0x00001000; // 4 KB

    // DDR3 RAM: 128 MB
    constexpr uint32_t RAM_BASE = 0x80000000;
    constexpr uint32_t RAM_SIZE = 0x08000000; // 128 MB

    // PLIC Interrupt Source IDs (spec Table 3, Section 4.1)
    constexpr uint32_t IRQ_UART = 1;
    constexpr uint32_t IRQ_GPIO = 2;
    constexpr uint32_t IRQ_TIMER = 3;
    constexpr uint32_t IRQ_SD = 4;
    constexpr uint32_t IRQ_DMA = 5;
    constexpr uint32_t IRQ_VIDEO = 6;
    constexpr uint32_t IRQ_AUDIO = 7;
    constexpr uint32_t IRQ_NUM_SOURCES = 8; // 0 is reserved

    // Framebuffer layout (spec Section 4.2)
    constexpr uint32_t FB0_DEFAULT = 0x84000000;
    constexpr uint32_t FB1_DEFAULT = 0x84010000;
    constexpr uint32_t PALETTE_DEFAULT = 0x84020000;
    constexpr uint32_t COLORMAP_DEFAULT = 0x84030000;

    // Audio ring buffer (spec Section 4.3)
    constexpr uint32_t AUDIO_RING_DEFAULT = 0x84040000;
    constexpr uint32_t AUDIO_RING_SIZE_DEFAULT = 0x1000; // 4 KB

    // Clock configuration
    constexpr uint32_t CPU_FREQ_HZ = 100000000;  // 100 MHz
    constexpr uint32_t CLINT_TICK_HZ = 10000000; // 10 MHz mtime tick rate

    // Temporal decoupling default quantum (spec Section 2.1.2)
    constexpr uint32_t DEFAULT_QUANTUM_US = 100; // 100 microseconds

    // HTIF tohost address for ISA compliance tests (spec Section 7.5)
    constexpr uint32_t TOHOST_ADDR = 0x80001000;

} // namespace cfg

#endif // GAMINGCPU_VP_PLATFORM_CONFIG_H
