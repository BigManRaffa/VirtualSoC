#include "bootrom.h"
#include <cstring>
#include <fstream>

BootROM::BootROM(sc_core::sc_module_name name, uint32_t base_addr, uint32_t size)
    : sc_module(name), tsock("tsock"), base_addr_(base_addr), size_(size), mem_(size, 0)
{
    tsock.register_b_transport(this, &BootROM::b_transport);
    tsock.register_get_direct_mem_ptr(this, &BootROM::get_direct_mem_ptr);
}

void BootROM::load_binary(const std::string &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        SC_REPORT_FATAL("BootROM", ("Cannot open binary file: " + path).c_str());
        return;
    }

    auto file_size = file.tellg();
    if (static_cast<uint32_t>(file_size) > size_)
    {
        SC_REPORT_FATAL("BootROM",
                        ("Binary file too large: " + std::to_string(file_size) +
                         " bytes, ROM is " + std::to_string(size_) + " bytes")
                            .c_str());
        return;
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(mem_.data()), file_size);
}

void BootROM::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay)
{
    tlm::tlm_command cmd = trans.get_command();
    uint32_t addr = static_cast<uint32_t>(trans.get_address());
    uint8_t *ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();

    if (addr + len > size_)
    {
        SC_REPORT_ERROR("BootROM", "Out-of-bounds access");
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND)
    {
        std::memcpy(ptr, &mem_[addr], len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    else if (cmd == tlm::TLM_WRITE_COMMAND)
    {
        // Spec 3.6.2: writes silently rejected with warning
        SC_REPORT_WARNING("BootROM", "Write to read-only BootROM ignored");
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    }
    else
    {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    }
}

bool BootROM::get_direct_mem_ptr(tlm::tlm_generic_payload &trans,
                                 tlm::tlm_dmi &dmi_data)
{
    // Spec 3.6.2: DMI granted with read-only permission
    dmi_data.set_dmi_ptr(mem_.data());
    dmi_data.set_start_address(0);
    dmi_data.set_end_address(size_ - 1);
    dmi_data.allow_read();
    dmi_data.set_read_latency(sc_core::SC_ZERO_TIME);
    dmi_data.set_write_latency(sc_core::SC_ZERO_TIME);
    return true;
}
