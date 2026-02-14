#include "rv32m.h"
#include <climits>

namespace rv32m {

uint32_t mul(uint32_t rs1, uint32_t rs2) {
    int64_t a = static_cast<int32_t>(rs1);
    int64_t b = static_cast<int32_t>(rs2);
    return static_cast<uint32_t>(a * b);
}

uint32_t mulh(uint32_t rs1, uint32_t rs2) {
    int64_t a = static_cast<int32_t>(rs1);
    int64_t b = static_cast<int32_t>(rs2);
    return static_cast<uint32_t>((a * b) >> 32);
}

uint32_t mulhsu(uint32_t rs1, uint32_t rs2) {
    int64_t a = static_cast<int32_t>(rs1);
    uint64_t b = rs2;
    return static_cast<uint32_t>((a * static_cast<int64_t>(b)) >> 32);
}

uint32_t mulhu(uint32_t rs1, uint32_t rs2) {
    uint64_t a = rs1;
    uint64_t b = rs2;
    return static_cast<uint32_t>((a * b) >> 32);
}

uint32_t div(uint32_t rs1, uint32_t rs2) {
    int32_t a = static_cast<int32_t>(rs1);
    int32_t b = static_cast<int32_t>(rs2);
    if (b == 0) return 0xFFFFFFFF;                        // div by zero -> -1
    if (a == INT32_MIN && b == -1) return uint32_t(INT32_MIN); // overflow -> -2^31
    return static_cast<uint32_t>(a / b);
}

uint32_t divu(uint32_t rs1, uint32_t rs2) {
    if (rs2 == 0) return 0xFFFFFFFF;
    return rs1 / rs2;
}

uint32_t rem(uint32_t rs1, uint32_t rs2) {
    int32_t a = static_cast<int32_t>(rs1);
    int32_t b = static_cast<int32_t>(rs2);
    if (b == 0) return rs1;                  // rem by zero -> dividend
    if (a == INT32_MIN && b == -1) return 0; // overflow -> 0
    return static_cast<uint32_t>(a % b);
}

uint32_t remu(uint32_t rs1, uint32_t rs2) {
    if (rs2 == 0) return rs1;
    return rs1 % rs2;
}

} // namespace rv32m
