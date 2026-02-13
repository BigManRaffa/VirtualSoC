#ifndef GAMINGCPU_VP_BOOTROM_H
#define GAMINGCPU_VP_BOOTROM_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <vector>
#include <string>

// Read-only memory initialized from a binary file at elaboration
// Replaces Gaming CPU RTL: bootrom.sv
// Writes are silently rejected (SC_WARNING). DMI granted read-only
class BootROM : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<BootROM> tsock;

    BootROM(sc_core::sc_module_name name, uint32_t base_addr, uint32_t size);

    SC_HAS_PROCESS(BootROM);

    // Load a raw binary image into the ROM, called during elaboration!
    void load_binary(const std::string &path);

    // Direct write access for ELF loader (bypasses read-only enforcement)
    uint8_t *data() { return mem_.data(); }
    const uint8_t *data() const { return mem_.data(); }

    uint32_t get_base_addr() const { return base_addr_; }
    uint32_t get_size() const { return size_; }

private:
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload &trans,
                            tlm::tlm_dmi &dmi_data);

    uint32_t base_addr_;
    uint32_t size_;
    std::vector<uint8_t> mem_;
};

#endif // GAMINGCPU_VP_BOOTROM_H
