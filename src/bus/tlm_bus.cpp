#include "tlm_bus.h"
#include <algorithm>
#include <sstream>

TLM_Bus::TLM_Bus(sc_core::sc_module_name name)
    : sc_module(name)
    , tsock("tsock")
    , isock("isock")
{
    tsock.register_b_transport(this, &TLM_Bus::b_transport);
    tsock.register_get_direct_mem_ptr(this, &TLM_Bus::get_direct_mem_ptr);
    isock.register_invalidate_direct_mem_ptr(this, &TLM_Bus::invalidate_direct_mem_ptr);
}

void TLM_Bus::map(uint32_t base, uint32_t size)
{
    uint32_t end = base + size;
    for (const auto& r : ranges_) {
        uint32_t r_end = r.base + r.size;
        if (base < r_end && r.base < end) {
            std::ostringstream oss;
            oss << "Overlapping address ranges: [0x" << std::hex << base
                << ", 0x" << end << ") overlaps [0x" << r.base
                << ", 0x" << r_end << ")";
            SC_REPORT_FATAL("TLM_Bus", oss.str().c_str());
        }
    }

    ranges_.push_back({base, size, next_target_idx_++});

    std::sort(ranges_.begin(), ranges_.end(),
              [](const MappedRange& a, const MappedRange& b) {
                  return a.base < b.base;
              });
}

int TLM_Bus::decode(uint32_t addr) const
{
    for (size_t i = 0; i < ranges_.size(); ++i) {
        if (addr >= ranges_[i].base &&
            addr < ranges_[i].base + ranges_[i].size) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TLM_Bus::b_transport(int id, tlm::tlm_generic_payload& trans,
                           sc_core::sc_time& delay)
{
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    int idx = decode(addr);

    if (idx < 0) {
        std::ostringstream oss;
        oss << "Address decode miss: 0x" << std::hex << addr;
        SC_REPORT_WARNING("TLM_Bus", oss.str().c_str());
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const MappedRange& range = ranges_[idx];

    trans.set_address(addr - range.base);
    isock[range.target_idx]->b_transport(trans, delay);
    trans.set_address(addr);
}

bool TLM_Bus::get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi& dmi_data)
{
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    int idx = decode(addr);

    if (idx < 0)
        return false;

    const MappedRange& range = ranges_[idx];

    trans.set_address(addr - range.base);
    bool ok = isock[range.target_idx]->get_direct_mem_ptr(trans, dmi_data);
    trans.set_address(addr);

    if (ok) {
        // Translate DMI range from target-local to global address space
        dmi_data.set_start_address(dmi_data.get_start_address() + range.base);
        dmi_data.set_end_address(dmi_data.get_end_address() + range.base);
    }

    return ok;
}

void TLM_Bus::invalidate_direct_mem_ptr(int id, sc_dt::uint64 start,
                                         sc_dt::uint64 end)
{
    for (const auto& range : ranges_) {
        if (range.target_idx == static_cast<uint32_t>(id)) {
            sc_dt::uint64 global_start = start + range.base;
            sc_dt::uint64 global_end = end + range.base;

            for (int i = 0; i < static_cast<int>(tsock.size()); ++i)
                tsock[i]->invalidate_direct_mem_ptr(global_start, global_end);
            return;
        }
    }
}
