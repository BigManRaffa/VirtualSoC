#ifndef GAMINGCPU_VP_RV32M_H
#define GAMINGCPU_VP_RV32M_H

#include <cstdint>

namespace rv32m {

uint32_t mul(uint32_t rs1, uint32_t rs2);
uint32_t mulh(uint32_t rs1, uint32_t rs2);
uint32_t mulhsu(uint32_t rs1, uint32_t rs2);
uint32_t mulhu(uint32_t rs1, uint32_t rs2);
uint32_t div(uint32_t rs1, uint32_t rs2);
uint32_t divu(uint32_t rs1, uint32_t rs2);
uint32_t rem(uint32_t rs1, uint32_t rs2);
uint32_t remu(uint32_t rs1, uint32_t rs2);

} // namespace rv32m

#endif // GAMINGCPU_VP_RV32M_H
