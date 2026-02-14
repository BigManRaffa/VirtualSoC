#ifndef GAMINGCPU_VP_CSR_H
#define GAMINGCPU_VP_CSR_H

#include <cstdint>
#include <functional>

class CSRFile {
public:
    CSRFile();

    // Returns false on privilege violation or non-existent CSR.
    bool read(uint16_t addr, uint8_t priv, uint32_t& val) const;
    bool write(uint16_t addr, uint8_t priv, uint32_t val);

    // ISS main loop increments
    void inc_mcycle()   { if (++mcycle == 0) ++mcycleh; }
    void inc_minstret() { if (++minstret == 0) ++minstreth; }

    // Hardware-driven mip bits (CLINT/PLIC set these, not software)
    void set_mip_mtip(bool v) { set_hw_bit(7, v); }
    void set_mip_msip(bool v) { set_hw_bit(3, v); }
    void set_mip_meip(bool v) { set_hw_bit(11, v); }
    void set_mip_seip(bool v) { set_hw_bit(9, v); }
    void set_mip_stip(bool v) { set_hw_bit(5, v); }
    void set_mip_ssip(bool v) { set_hw_bit(1, v); }
    uint32_t get_mip() const { return sw_mip | hw_mip; }

    // satp write callback (triggers TLB flush in MMU)
    std::function<void()> on_satp_write;

    // Direct access for trap handler / ISS
    uint32_t mstatus    = 0;
    uint32_t misa       = 0;
    uint32_t medeleg    = 0;
    uint32_t mideleg    = 0;
    uint32_t mie        = 0;
    uint32_t mtvec      = 0;
    uint32_t mcounteren = 0;
    uint32_t mscratch   = 0;
    uint32_t mepc       = 0;
    uint32_t mcause     = 0;
    uint32_t mtval      = 0;

    uint32_t stvec      = 0;
    uint32_t scounteren = 0;
    uint32_t sscratch   = 0;
    uint32_t sepc       = 0;
    uint32_t scause     = 0;
    uint32_t stval      = 0;
    uint32_t satp       = 0;

private:
    uint32_t mcycle     = 0;
    uint32_t mcycleh    = 0;
    uint32_t minstret   = 0;
    uint32_t minstreth  = 0;
    uint32_t hw_mip     = 0; // bits driven by hardware (CLINT/PLIC)
    uint32_t sw_mip     = 0; // bits writable by software (SSIP only)

    void set_hw_bit(int bit, bool v) {
        if (v) hw_mip |= (1u << bit); else hw_mip &= ~(1u << bit);
    }

    // WARL mask: only these mstatus bits are writable
    static constexpr uint32_t MSTATUS_WRITE_MASK =
        (1 << 1)  | (1 << 3)  | (1 << 5)  | (1 << 7)  | // SIE MIE SPIE MPIE
        (1 << 8)  | (3 << 11) |                           // SPP MPP
        (1 << 17) | (1 << 18) | (1 << 19) |               // MPRV SUM MXR
        (1 << 20) | (1 << 21) | (1 << 22);                // TVM TW TSR

    // S-mode sees only these mstatus bits
    static constexpr uint32_t SSTATUS_MASK =
        (1 << 1) | (1 << 5) | (1 << 8) | (1 << 18) | (1 << 19);

    // S-mode can see/write these mie/mip bits
    static constexpr uint32_t S_INT_MASK =
        (1 << 1) | (1 << 5) | (1 << 9); // SSIP STIP SEIP
};

#endif // GAMINGCPU_VP_CSR_H
