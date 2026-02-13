// GamingCPU Virtual Platform — sc_main entry point
// Spec Section 3.2: Parses args, instantiates platform, loads ELF, runs simulation
//
// Current state: Step 1 smoke test, instantiates Memory and BootROM, performs basic TLM transactions and DMI verification
// Will be replaced with full platform wiring as modules are added

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <cstring>

#include "mem/memory.h"
#include "mem/bootrom.h"
#include "platform/platform_config.h"

// Minimal test initiator to drive TLM transactions against memory targets
struct TestInitiator : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<TestInitiator> isock;

    SC_HAS_PROCESS(TestInitiator);

    TestInitiator(sc_core::sc_module_name name)
        : sc_module(name), isock("isock")
    {
        SC_THREAD(run_tests);
    }

    void run_tests() {
        test_memory_rw();
        test_memory_dmi();
        test_bootrom_read_only();
        test_bootrom_dmi_read_only();

        std::cout << "\nAll Step 1 tests passed!\n" << std::endl;
        sc_core::sc_stop();
    }

    void test_memory_rw() {
        std::cout << "[TEST] Memory read/write via TLM b_transport..." << std::flush;

        // Write 4 bytes at offset 0x100
        uint32_t write_data = 0xDEADBEEF;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x100);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&write_data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        isock->b_transport(trans, delay);
        sc_assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);

        // Read back
        uint32_t read_data = 0;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0x100);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&read_data));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        isock->b_transport(trans, delay);
        sc_assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
        sc_assert(read_data == 0xDEADBEEF);

        std::cout << " PASS" << std::endl;
    }

    void test_memory_dmi() {
        std::cout << "[TEST] Memory DMI pointer acquisition..." << std::flush;

        tlm::tlm_generic_payload trans;
        tlm::tlm_dmi dmi_data;

        trans.set_address(0);
        bool ok = isock->get_direct_mem_ptr(trans, dmi_data);
        sc_assert(ok);
        sc_assert(dmi_data.get_dmi_ptr() != nullptr);
        sc_assert(dmi_data.is_read_allowed());
        sc_assert(dmi_data.is_write_allowed());

        // Verifying if the data we wrote via b_transport is visible through DMI
        uint8_t* dmi_ptr = dmi_data.get_dmi_ptr();
        uint32_t val;
        std::memcpy(&val, dmi_ptr + 0x100, 4);
        sc_assert(val == 0xDEADBEEF);

        // Write via DMI, read back via b_transport
        uint32_t dmi_write = 0xCAFEBABE;
        std::memcpy(dmi_ptr + 0x200, &dmi_write, 4);

        uint32_t read_back = 0;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0x200);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&read_back));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(trans, delay);
        sc_assert(read_back == 0xCAFEBABE);

        std::cout << " PASS" << std::endl;
    }

    void test_bootrom_read_only() {
        // This test uses a separate socket binding
        // Tested via the top-level wiring below (bootrom is bound to a second initiator).
        std::cout << "[TEST] BootROM read-only enforcement..." << std::flush;
        std::cout << " PASS (tested via separate initiator)" << std::endl;
    }

    void test_bootrom_dmi_read_only() {
        std::cout << "[TEST] BootROM DMI read-only permission..." << std::flush;
        std::cout << " PASS (tested via separate initiator)" << std::endl;
    }
};

// Second initiator to test BootROM independently
struct BootROMTester : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<BootROMTester> isock;

    SC_HAS_PROCESS(BootROMTester);

    BootROMTester(sc_core::sc_module_name name)
        : sc_module(name), isock("isock")
    {
        SC_THREAD(run_tests);
    }

    void run_tests() {
        // Test read from BootROM (pre-filled with a pattern)
        std::cout << "[TEST] BootROM read via TLM..." << std::flush;

        uint32_t read_data = 0;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&read_data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        isock->b_transport(trans, delay);
        sc_assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
        // First 4 bytes were set to 0x13 0x00 0x00 0x00 (RISC-V NOP = addi x0,x0,0)
        sc_assert(read_data == 0x00000013);
        std::cout << " PASS" << std::endl;

        // Test write rejected
        std::cout << "[TEST] BootROM write rejection..." << std::flush;
        uint32_t write_data = 0xBAADF00D;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&write_data));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        isock->b_transport(trans, delay);
        sc_assert(trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE);
        std::cout << " PASS" << std::endl;

        // Verifying if data unchanged after rejected write
        std::cout << "[TEST] BootROM data unchanged after write..." << std::flush;
        uint32_t verify = 0;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&verify));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(trans, delay);
        sc_assert(verify == 0x00000013);
        std::cout << " PASS" << std::endl;

        // Test DMI is read-only
        std::cout << "[TEST] BootROM DMI read-only permission..." << std::flush;
        tlm::tlm_dmi dmi_data;
        trans.set_address(0);
        bool ok = isock->get_direct_mem_ptr(trans, dmi_data);
        sc_assert(ok);
        sc_assert(dmi_data.is_read_allowed());
        sc_assert(!dmi_data.is_write_allowed());
        std::cout << " PASS" << std::endl;
    }
};

int sc_main(int argc, char* argv[])
{
    std::cout << "[VP] GamingCPU Virtual Platform — Step 1 Test" << std::endl;
    std::cout << "[VP] SystemC " << sc_core::sc_version() << std::endl;

    // Instantiate memory: 1 MB for testing (full 128 MB not needed yet)
    Memory ram("ram", cfg::RAM_BASE, 0x100000);

    // Instantiate BootROM: 64 KB
    BootROM bootrom("bootrom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);

    // Pre-fill BootROM with a RISC-V NOP instruction at offset 0
    // (Normally loaded from a binary file, here im just writing directly for testing)
    uint32_t nop = 0x00000013;  // addi x0, x0, 0
    std::memcpy(bootrom.data(), &nop, sizeof(nop));

    // Instantiate test initiators
    TestInitiator tester("tester");
    BootROMTester rom_tester("rom_tester");

    // Bind: tester -> ram, rom_tester -> bootrom
    tester.isock.bind(ram.tsock);
    rom_tester.isock.bind(bootrom.tsock);

    // Run simulation
    sc_core::sc_start();

    std::cout << "[VP] Simulation complete." << std::endl;
    return 0;
}
