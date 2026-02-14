#ifndef GAMINGCPU_VP_RV32A_H
#define GAMINGCPU_VP_RV32A_H

#include <cstdint>
#include <algorithm>

struct Reservation {
    uint32_t addr = 0;
    bool valid = false;

    void set(uint32_t a)   { addr = a & ~0x3u; valid = true; }
    void clear()           { valid = false; }
    bool check(uint32_t a) const { return valid && addr == (a & ~0x3u); }
};

namespace rv32a {

inline uint32_t amo_swap(uint32_t mem, uint32_t rs2) { return rs2; }
inline uint32_t amo_add(uint32_t mem, uint32_t rs2)  { return mem + rs2; }
inline uint32_t amo_xor(uint32_t mem, uint32_t rs2)  { return mem ^ rs2; }
inline uint32_t amo_and(uint32_t mem, uint32_t rs2)  { return mem & rs2; }
inline uint32_t amo_or(uint32_t mem, uint32_t rs2)   { return mem | rs2; }

inline uint32_t amo_min(uint32_t mem, uint32_t rs2) {
    return static_cast<uint32_t>(
        std::min(static_cast<int32_t>(mem), static_cast<int32_t>(rs2)));
}
inline uint32_t amo_max(uint32_t mem, uint32_t rs2) {
    return static_cast<uint32_t>(
        std::max(static_cast<int32_t>(mem), static_cast<int32_t>(rs2)));
}
inline uint32_t amo_minu(uint32_t mem, uint32_t rs2) { return std::min(mem, rs2); }
inline uint32_t amo_maxu(uint32_t mem, uint32_t rs2) { return std::max(mem, rs2); }

} // namespace rv32a

#endif // GAMINGCPU_VP_RV32A_H
