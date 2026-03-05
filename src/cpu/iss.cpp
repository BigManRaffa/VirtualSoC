#include "iss.h"
#include "decode.h"
#include "rv32_defs.h"
#include "platform/platform_config.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <cstring>

ISS::ISS(sc_core::sc_module_name name, uint32_t reset_pc)
    : sc_module(name)
    , isock("isock")
    , reset_pc_(reset_pc)
    , clk_period_(10, sc_core::SC_NS)
{
    SC_THREAD(run);

    isock.register_invalidate_direct_mem_ptr(this, &ISS::invalidate_dmi);

    mmu.mem_read = [this](uint32_t paddr) -> uint32_t {
        return bus_read(paddr, 4);
    };
    mmu.mem_write = [this](uint32_t paddr, uint32_t val) {
        bus_write(paddr, val, 4);
    };

    state.csr.on_satp_write = [this]() { mmu.flush_tlb(); };

    state.mem.read = [this](uint32_t vaddr, int bytes) -> uint32_t {
        uint32_t paddr = vaddr;
        if (mmu_active_data()) {
            auto r = mmu.translate(vaddr, AccessType::LOAD,
                                   effective_data_priv(),
                                   state.csr.satp, state.csr.mstatus);
            if (r.fault) {
                mem_fault_ = true;
                mem_fault_cause_ = r.cause;
                mem_fault_vaddr_ = vaddr;
                return 0;
            }
            paddr = r.paddr;
        }
        return bus_read(paddr, bytes);
    };

    state.mem.write = [this](uint32_t vaddr, uint32_t data, int bytes) {
        uint32_t paddr = vaddr;
        if (mmu_active_data()) {
            auto r = mmu.translate(vaddr, AccessType::STORE,
                                   effective_data_priv(),
                                   state.csr.satp, state.csr.mstatus);
            if (r.fault) {
                mem_fault_ = true;
                mem_fault_cause_ = r.cause;
                mem_fault_vaddr_ = vaddr;
                return;
            }
            paddr = r.paddr;
        }
        bus_write(paddr, data, bytes);
    };
}

bool ISS::mmu_active_fetch() const {
    return (state.csr.satp >> 31) != 0 && state.priv < 3;
}

bool ISS::mmu_active_data() const {
    return (state.csr.satp >> 31) != 0 && effective_data_priv() < 3;
}

// M-mode with MPRV uses MPP for data access privilege
uint8_t ISS::effective_data_priv() const {
    if (state.priv == 3 && (state.csr.mstatus & rv32::MSTATUS_MPRV))
        return (state.csr.mstatus >> rv32::MSTATUS_MPP_SHIFT) & 0x3;
    return state.priv;
}

void ISS::run() {
    tlm_utils::tlm_quantumkeeper qk;
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(cfg::DEFAULT_QUANTUM_US, sc_core::SC_US));
    qk.reset();

    state.pc = reset_pc_;

    while (true) {
        uint32_t irq = trap::check_pending_interrupts(state);
        if (irq) {
            trap::take_trap(state, irq, 0);
            state.pc = state.next_pc;
            continue;
        }

        if (state.pc & 1) {
            trap::take_trap(state, rv32::CAUSE_MISALIGNED_FETCH, state.pc);
            state.pc = state.next_pc;
            continue;
        }

        uint32_t fetch_paddr = state.pc;
        if (mmu_active_fetch()) {
            auto r = mmu.translate(state.pc, AccessType::FETCH, state.priv,
                                   state.csr.satp, state.csr.mstatus);
            if (r.fault) {
                trap::take_trap(state, r.cause, state.pc);
                state.pc = state.next_pc;
                continue;
            }
            fetch_paddr = r.paddr;
        }
        uint32_t raw = bus_read(fetch_paddr, 4);

        DecodedInstr d = decode(raw);
        state.next_pc = state.pc + d.instr_len();

        mem_fault_ = false;
        ExecResult r = execute(state, d);

        if (mem_fault_) {
            mem_fault_ = false;
            trap::take_trap(state, mem_fault_cause_, mem_fault_vaddr_);
            state.pc = state.next_pc;
            qk.inc(clk_period_);
            if (qk.need_sync()) qk.sync();
            continue;
        }

        insn_count++;
        state.csr.inc_mcycle();
        state.csr.inc_minstret();

        // 7. Handle execution exceptions
        if (r.exception) {
            if (stop_on_ebreak && r.cause == rv32::CAUSE_BREAKPOINT)
                return;
            trap::take_trap(state, r.cause, r.tval);
        }

        // 8. Handle special instructions
        if (r.wfi) {
            qk.sync();
            wait(sc_core::sc_time(cfg::DEFAULT_QUANTUM_US, sc_core::SC_US),
                 wfi_event_);
            qk.reset();
        }
        if (r.fence_i) {
            dmi_valid_ = false;
            dmi_ptr_ = nullptr;
        }
        if (r.sfence_vma)
            mmu.flush_tlb();

        state.pc = state.next_pc;

        // 9. Advance time
        qk.inc(clk_period_);
        if (qk.need_sync())
            qk.sync();
    }
}

uint32_t ISS::bus_read(uint32_t addr, int bytes) {
    if (dmi_valid_ && addr >= dmi_start_ && (addr + bytes - 1) <= dmi_end_) {
        uint32_t v = 0;
        std::memcpy(&v, dmi_ptr_ + (addr - dmi_start_), bytes);
        return v;
    }

    uint8_t buf[4] = {};
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf);
    trans.set_data_length(bytes);
    trans.set_streaming_width(bytes);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    isock->b_transport(trans, delay);

    if (!dmi_valid_)
        try_dmi(addr);

    uint32_t v = 0;
    std::memcpy(&v, buf, bytes);
    return v;
}

void ISS::bus_write(uint32_t addr, uint32_t data, int bytes) {
    if (dmi_valid_ && addr >= dmi_start_ && (addr + bytes - 1) <= dmi_end_) {
        std::memcpy(dmi_ptr_ + (addr - dmi_start_), &data, bytes);
        return;
    }

    uint8_t buf[4] = {};
    std::memcpy(buf, &data, bytes);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(buf);
    trans.set_data_length(bytes);
    trans.set_streaming_width(bytes);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    isock->b_transport(trans, delay);
}

void ISS::try_dmi(uint32_t addr) {
    tlm::tlm_generic_payload trans;
    trans.set_address(addr);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(0);
    trans.set_data_ptr(nullptr);

    tlm::tlm_dmi dmi_data;
    if (isock->get_direct_mem_ptr(trans, dmi_data)) {
        dmi_valid_ = true;
        dmi_ptr_ = dmi_data.get_dmi_ptr();
        dmi_start_ = dmi_data.get_start_address();
        dmi_end_ = dmi_data.get_end_address();
    }
}

void ISS::invalidate_dmi(sc_dt::uint64 start, sc_dt::uint64 end) {
    if (dmi_valid_ && !(end < dmi_start_ || start > dmi_end_)) {
        dmi_valid_ = false;
        dmi_ptr_ = nullptr;
    }
}
