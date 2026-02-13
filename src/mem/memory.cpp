#include "memory.h"
#include <cstring>

Memory::Memory(sc_core::sc_module_name name, uint32_t base_addr, uint32_t size)
    : sc_module(name)
    , tsock("tsock")
    , base_addr_(base_addr)
    , size_(size)
    , mem_(size, 0)
{
    tsock.register_b_transport(this, &Memory::b_transport);
    tsock.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
}

void Memory::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    tlm::tlm_command cmd = trans.get_command();
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();

    // Bounds check! Your address is already bus-adjusted (peripheral-local offset :P)
    if (addr + len > size_) {
        SC_REPORT_ERROR("Memory", "Out-of-bounds access");
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        std::memcpy(ptr, &mem_[addr], len);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&mem_[addr], ptr, len);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

bool Memory::get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                tlm::tlm_dmi& dmi_data)
{
    // Grant full read/write DMI access to the entire memory region
    // The ISS caches this pointer for fast instruction fetch and data access so were bypassing the TLM socket path entirely 
    // Aka the biggest perf optimization
    dmi_data.set_dmi_ptr(mem_.data());
    dmi_data.set_start_address(0);
    dmi_data.set_end_address(size_ - 1);
    dmi_data.allow_read_write();
    dmi_data.set_read_latency(sc_core::SC_ZERO_TIME);
    dmi_data.set_write_latency(sc_core::SC_ZERO_TIME);
    return true;
}
