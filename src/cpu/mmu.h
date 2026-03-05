#ifndef GAMINGCPU_VP_MMU_H
#define GAMINGCPU_VP_MMU_H

#include <cstdint>
#include <functional>
#include <unordered_map>
#include "rv32_defs.h"

enum class AccessType { FETCH, LOAD, STORE };

struct MMUResult {
    uint32_t paddr = 0;
    bool fault = false;
    uint32_t cause = 0;
};

class MMU
{
public:
    std::function<uint32_t(uint32_t)> mem_read;
    std::function<void(uint32_t, uint32_t)> mem_write;

    MMUResult translate(uint32_t vaddr, AccessType type, uint8_t priv,
                        uint32_t satp, uint32_t mstatus);

    void flush_tlb();

private:
    struct TLBEntry {
        uint32_t ppn;
        uint32_t pte_flags;
        bool is_superpage;
    };

    static constexpr size_t TLB_SIZE = 64;
    std::unordered_map<uint32_t, TLBEntry> tlb_;

    MMUResult walk(uint32_t vaddr, AccessType type, uint8_t priv,
                   uint32_t satp, uint32_t mstatus);

    bool check_permissions(uint32_t pte, AccessType type, uint8_t priv,
                           uint32_t mstatus);
};

#endif // GAMINGCPU_VP_MMU_H
