// GamingCPU Virtual Platform -- sc_main entry point
// Spec Section 3.2: Parses args, instantiates platform, loads ELF, runs simulation.
//
// Current state: Step 2 smoke test -- routes transactions through the TLM bus
// to Memory and BootROM targets. Tests address decode, offset adjustment, and DMI
// forwarding through the bus.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <cstring>

#include "mem/memory.h"
#include "mem/bootrom.h"
#include "bus/tlm_bus.h"
#include "platform/platform_config.h"

// Helper to build a TLM transaction
static void setup_trans(tlm::tlm_generic_payload& trans,
                        tlm::tlm_command cmd, uint32_t addr,
                        uint8_t* data, uint32_t len)
{
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
}

// Test initiator -- drives transactions through the bus using global addresses
struct TestInitiator : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<TestInitiator> isock;

    SC_HAS_PROCESS(TestInitiator);

    TestInitiator(sc_core::sc_module_name name)
        : sc_module(name), isock("isock")
    {
        SC_THREAD(run_tests);
    }

    void run_tests() {
        int pass = 0;
        int fail = 0;

        auto check = [&](bool cond, const char* name) {
            if (cond) {
                std::cout << "[TEST] " << name << "... PASS" << std::endl;
                ++pass;
            } else {
                std::cout << "[TEST] " << name << "... FAIL" << std::endl;
                ++fail;
            }
        };

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        // --- RAM via bus (global address 0x80000000+) ---
        {
            uint32_t wdata = 0xDEADBEEF;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::RAM_BASE + 0x100, reinterpret_cast<uint8_t*>(&wdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
                  "RAM write via bus (global addr 0x80000100)");

            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::RAM_BASE + 0x100, reinterpret_cast<uint8_t*>(&rdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0xDEADBEEF,
                  "RAM read-back via bus");
        }

        // --- BootROM via bus (global address 0x00000000+) ---
        {
            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::BOOTROM_BASE, reinterpret_cast<uint8_t*>(&rdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0x00000013,
                  "BootROM read via bus (global addr 0x00000000)");

            // Write should be rejected
            uint32_t wdata = 0xBAADF00D;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::BOOTROM_BASE, reinterpret_cast<uint8_t*>(&wdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE,
                  "BootROM write rejection via bus");
        }

        // Address decode miss
        {
            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        0x50000000, reinterpret_cast<uint8_t*>(&rdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "Address decode miss (unmapped 0x50000000)");
        }

        // DMI through bus for RAM (global address space)
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, cfg::RAM_BASE, nullptr, 0);
            bool ok = isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok && dmi_data.get_dmi_ptr() != nullptr,
                  "RAM DMI acquisition through bus");
            check(dmi_data.get_start_address() == cfg::RAM_BASE,
                  "RAM DMI start address is global (0x80000000)");
            check(dmi_data.is_read_allowed() && dmi_data.is_write_allowed(),
                  "RAM DMI has R/W permission");

            // Verify DMI coherence: data written via b_transport visible through DMI
            if (ok) {
                uint8_t* dmi_ptr = dmi_data.get_dmi_ptr();
                uint32_t val;
                std::memcpy(&val, dmi_ptr + 0x100, 4);
                check(val == 0xDEADBEEF,
                      "RAM DMI coherence (b_transport write visible via DMI ptr)");
            }
        }

        // DMI through bus for BootROM (read-only)
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, cfg::BOOTROM_BASE, nullptr, 0);
            bool ok = isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok, "BootROM DMI acquisition through bus");
            check(dmi_data.get_start_address() == cfg::BOOTROM_BASE,
                  "BootROM DMI start address is global (0x00000000)");
            check(dmi_data.is_read_allowed() && !dmi_data.is_write_allowed(),
                  "BootROM DMI is read-only");
        }

        // SRAM via bus (second memory region at 0x01000000)
        {
            uint32_t wdata = 0xCAFEBABE;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::SRAM_BASE + 0x10, reinterpret_cast<uint8_t*>(&wdata), 4);
            isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
                  "SRAM write via bus (global addr 0x01000010)");

            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::SRAM_BASE + 0x10, reinterpret_cast<uint8_t*>(&rdata), 4);
            isock->b_transport(trans, delay);
            check(rdata == 0xCAFEBABE,
                  "SRAM read-back via bus (different region from DDR)");
        }

        std::cout << "\n=== Step 2 Results: " << pass << " passed, "
                  << fail << " failed ===" << std::endl;

        if (fail > 0)
            SC_REPORT_FATAL("Test", "Some tests failed");

        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    std::cout << "[VP] GamingCPU Virtual Platform -- Step 2 Test (Bus Routing)"
              << std::endl;

    // Instantiate component
    TLM_Bus bus("bus");

    // DDR RAM: 1 MB for testing (128 MB in production)
    Memory ram("ram", cfg::RAM_BASE, 0x100000);

    // On-chip SRAM: 64 KB
    Memory sram("sram", cfg::SRAM_BASE, cfg::SRAM_SIZE);

    // BootROM: 64 KB
    BootROM bootrom("bootrom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);

    // Pre-fill BootROM with a RISC-V NOP
    uint32_t nop = 0x00000013;
    std::memcpy(bootrom.data(), &nop, sizeof(nop));

    // Test initiator (acts as CPU)
    TestInitiator tester("tester");

    // Wiring: initiator -> bus -> targets
    // Master side: tester's initiator socket -> bus target socket
    tester.isock.bind(bus.tsock);

    // Slave side: bus initiator socket -> each target's socket
    // bind() and map() must be called in matching order
    bus.isock.bind(bootrom.tsock);
    bus.map(cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);

    bus.isock.bind(sram.tsock);
    bus.map(cfg::SRAM_BASE, cfg::SRAM_SIZE);

    bus.isock.bind(ram.tsock);
    bus.map(cfg::RAM_BASE, 0x100000);  // 1 MB for testing

    // NOW WE RUN IT!!!
    sc_core::sc_start();

    std::cout << "[VP] Simulation complete." << std::endl;
    return 0;
}
