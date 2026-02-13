#ifndef GAMINGCPU_VP_TLM_BUS_H
#define GAMINGCPU_VP_TLM_BUS_H

#include <systemc>
#include <tlm>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/multi_passthrough_initiator_socket.h>
#include <cstdint>
#include <vector>
#include <string>

// TLM-2.0 address-routing bus, Replaces RTL axi_crossbar.sv
class TLM_Bus : public sc_core::sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<TLM_Bus> tsock;   // masters bind here
    tlm_utils::multi_passthrough_initiator_socket<TLM_Bus> isock; // binds to slaves

    TLM_Bus(sc_core::sc_module_name name);
    SC_HAS_PROCESS(TLM_Bus);

    // Register address range for the next bound target, Call in same order as bind()
    void map(uint32_t base, uint32_t size);

private:
    struct MappedRange {
        uint32_t base;
        uint32_t size;
        uint32_t target_idx;
    };

    std::vector<MappedRange> ranges_;
    uint32_t next_target_idx_ = 0;

    int decode(uint32_t addr) const;

    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data);
    void invalidate_direct_mem_ptr(int id, sc_dt::uint64 start, sc_dt::uint64 end);
};

#endif // GAMINGCPU_VP_TLM_BUS_H
