#ifndef GAMINGCPU_VP_MEMORY_H
#define GAMINGCPU_VP_MEMORY_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <vector>

// Unified RAM model (on-chip SRAM + DDR3 merged into flat array)
// Replaces the following GamingCPU RTL: sram_dualport.sv, MIG DDR3 controller, cache hierarchy
// Supports TLM blocking transport and DMI for zero-copy access
class Memory : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<Memory> tsock;

    Memory(sc_core::sc_module_name name, uint32_t base_addr, uint32_t size);

    SC_HAS_PROCESS(Memory);

    uint32_t get_base_addr() const { return base_addr_; }
    uint32_t get_size() const { return size_; }

    // Direct pointer access for ELF loading and test harnesses slop
    uint8_t *data() { return mem_.data(); }
    const uint8_t *data() const { return mem_.data(); }

private:
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload &trans,
                            tlm::tlm_dmi &dmi_data);

    uint32_t base_addr_;
    uint32_t size_;
    std::vector<uint8_t> mem_;
};

#endif // GAMINGCPU_VP_MEMORY_H
