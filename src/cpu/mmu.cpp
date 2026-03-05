#include "mmu.h"

// Sv32: [VPN1(10) | VPN0(10) | offset(12)]
// PTE:  [PPN1(12) | PPN0(10) | RSW(2) | D A G U X W R V]

static constexpr uint32_t PAGE_SHIFT = 12;
static constexpr uint32_t PAGE_SIZE = 1u << PAGE_SHIFT;
static constexpr uint32_t VPN_BITS = 10;
static constexpr uint32_t VPN_MASK = (1u << VPN_BITS) - 1;

static uint32_t fault_cause(AccessType type) {
    switch (type) {
    case AccessType::FETCH: return rv32::CAUSE_FETCH_PAGE_FAULT;
    case AccessType::LOAD:  return rv32::CAUSE_LOAD_PAGE_FAULT;
    case AccessType::STORE: return rv32::CAUSE_STORE_PAGE_FAULT;
    }
    return rv32::CAUSE_LOAD_PAGE_FAULT;
}

bool MMU::check_permissions(uint32_t pte, AccessType type, uint8_t priv,
                            uint32_t mstatus) {
    bool user_page = (pte & rv32::PTE_U) != 0;

    if (priv == rv32::PRV_U && !user_page)
        return false;

    // S-mode touching U pages without SUM? nope
    if (priv == rv32::PRV_S && user_page && !(mstatus & rv32::MSTATUS_SUM))
        return false;

    switch (type) {
    case AccessType::FETCH:
        // S-mode can NEVER execute U pages, even with SUM. spec says so!
        if (priv == rv32::PRV_S && user_page)
            return false;
        return (pte & rv32::PTE_X) != 0;

    case AccessType::LOAD:
        if (mstatus & rv32::MSTATUS_MXR)
            return (pte & (rv32::PTE_R | rv32::PTE_X)) != 0;
        return (pte & rv32::PTE_R) != 0;

    case AccessType::STORE:
        return (pte & rv32::PTE_W) != 0;
    }
    return false;
}

MMUResult MMU::translate(uint32_t vaddr, AccessType type, uint8_t priv,
                         uint32_t satp, uint32_t mstatus) {
    uint32_t mode = satp >> rv32::SATP_MODE_SHIFT;
    if (mode == rv32::SATP_MODE_BARE)
        return { vaddr, false, 0 };

    uint32_t vpn = vaddr >> PAGE_SHIFT;
    auto it = tlb_.find(vpn);
    if (it != tlb_.end()) {
        auto& e = it->second;
        if (check_permissions(e.pte_flags, type, priv, mstatus)) {
            uint32_t paddr;
            if (e.is_superpage)
                paddr = (e.ppn & ~0x3FFu) << PAGE_SHIFT | (vaddr & 0x003FFFFF);
            else
                paddr = e.ppn << PAGE_SHIFT | (vaddr & (PAGE_SIZE - 1));
            return { paddr, false, 0 };
        }
        return { 0, true, fault_cause(type) };
    }

    return walk(vaddr, type, priv, satp, mstatus);
}

MMUResult MMU::walk(uint32_t vaddr, AccessType type, uint8_t priv,
                    uint32_t satp, uint32_t mstatus) {
    uint32_t root_ppn = satp & rv32::SATP_PPN_MASK;
    uint32_t vpn[2] = {
        (vaddr >> PAGE_SHIFT) & VPN_MASK,
        (vaddr >> (PAGE_SHIFT + VPN_BITS)) & VPN_MASK
    };

    uint32_t a = root_ppn * PAGE_SIZE;
    int level = 1;

    for (int depth = 0; depth < 2; depth++) {
        uint32_t pte_addr = a + vpn[level] * 4;
        uint32_t pte = mem_read(pte_addr);

        if (!(pte & rv32::PTE_V))
            return { 0, true, fault_cause(type) };

        // R=0 W=1 is reserved. the spec explicitly says this is illegal
        if (!(pte & rv32::PTE_R) && (pte & rv32::PTE_W))
            return { 0, true, fault_cause(type) };

        bool is_leaf = (pte & (rv32::PTE_R | rv32::PTE_W | rv32::PTE_X)) != 0;

        if (is_leaf) {
            if (!check_permissions(pte, type, priv, mstatus))
                return { 0, true, fault_cause(type) };

            uint32_t ppn = pte >> rv32::PTE_PPN_SHIFT;
            bool is_superpage = (level == 1);

            // 4MB superpage PPN[0] must be zero or it's misaligned
            if (is_superpage && (ppn & VPN_MASK) != 0)
                return { 0, true, fault_cause(type) };

            // Hardware A/D bit update
            uint32_t required = rv32::PTE_A;
            if (type == AccessType::STORE)
                required |= rv32::PTE_D;

            if ((pte & required) != required) {
                pte |= required;
                mem_write(pte_addr, pte);
            }

            uint32_t paddr;
            if (is_superpage)
                paddr = (ppn & ~VPN_MASK) << PAGE_SHIFT | (vaddr & 0x003FFFFF);
            else
                paddr = ppn << PAGE_SHIFT | (vaddr & (PAGE_SIZE - 1));

            uint32_t vpn_key = vaddr >> PAGE_SHIFT;
            if (tlb_.size() >= TLB_SIZE)
                tlb_.erase(tlb_.begin());
            tlb_[vpn_key] = { ppn, pte & 0xFF, is_superpage };

            return { paddr, false, 0 };
        }

        if (level == 0)
            return { 0, true, fault_cause(type) };

        a = (pte >> rv32::PTE_PPN_SHIFT) * PAGE_SIZE;
        level--;
    }

    return { 0, true, fault_cause(type) };
}

void MMU::flush_tlb() {
    tlb_.clear();
}
