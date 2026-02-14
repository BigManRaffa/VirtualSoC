// GamingCPU Virtual Platform -- sc_main entry point
// Runs all step tests cumulatively in order.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <cstring>

#include "mem/memory.h"
#include "mem/bootrom.h"
#include "bus/tlm_bus.h"
#include "platform/platform_config.h"
#include "cpu/decode.h"
#include "cpu/rv32_defs.h"

static int pass_count = 0;
static int fail_count = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::cout << "[TEST] " << name << "... PASS\n";
        ++pass_count;
    } else {
        std::cout << "[TEST] " << name << "... FAIL\n";
        ++fail_count;
    }
}

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

// Initiator that drives Step 1 (memory) and Step 2 (bus) tests via b_transport,
// then calls pure-C++ Step 3 and Step 4 tests after.
struct TestInitiator : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<TestInitiator> mem_isock;
    tlm_utils::simple_initiator_socket<TestInitiator> rom_isock;
    tlm_utils::simple_initiator_socket<TestInitiator> bus_isock;

    SC_HAS_PROCESS(TestInitiator);

    TestInitiator(sc_core::sc_module_name name)
        : sc_module(name),
          mem_isock("mem_isock"),
          rom_isock("rom_isock"),
          bus_isock("bus_isock")
    {
        SC_THREAD(run_tests);
    }

    void step1_memory() {
        std::cout << "\n--- Step 1: Memory ---\n";
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        // Direct RAM write/read (no bus)
        {
            uint32_t wdata = 0xDEADBEEF;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        0x100, reinterpret_cast<uint8_t*>(&wdata), 4);
            mem_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE, "RAM direct write");

            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        0x100, reinterpret_cast<uint8_t*>(&rdata), 4);
            mem_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0xDEADBEEF,
                  "RAM direct read-back");
        }

        // RAM DMI
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, 0, nullptr, 0);
            bool ok = mem_isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok && dmi_data.get_dmi_ptr() != nullptr, "RAM DMI acquisition");
            check(dmi_data.is_read_allowed() && dmi_data.is_write_allowed(), "RAM DMI R/W");

            if (ok) {
                uint32_t val;
                std::memcpy(&val, dmi_data.get_dmi_ptr() + 0x100, 4);
                check(val == 0xDEADBEEF, "RAM DMI coherence");
            }
        }

        // BootROM read
        {
            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        0, reinterpret_cast<uint8_t*>(&rdata), 4);
            rom_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0x00000013,
                  "BootROM direct read");
        }

        // BootROM write rejection
        {
            uint32_t wdata = 0xBAADF00D;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        0, reinterpret_cast<uint8_t*>(&wdata), 4);
            rom_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE,
                  "BootROM write rejection");
        }

        // BootROM DMI (read-only)
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, 0, nullptr, 0);
            bool ok = rom_isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok, "BootROM DMI acquisition");
            check(dmi_data.is_read_allowed() && !dmi_data.is_write_allowed(), "BootROM DMI read-only");
        }
    }

    void step2_bus() {
        std::cout << "\n--- Step 2: Bus ---\n";
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        // RAM via bus
        {
            uint32_t wdata = 0xCAFEBABE;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::RAM_BASE + 0x200, reinterpret_cast<uint8_t*>(&wdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE, "RAM write via bus");

            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::RAM_BASE + 0x200, reinterpret_cast<uint8_t*>(&rdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0xCAFEBABE,
                  "RAM read-back via bus");
        }

        // BootROM via bus
        {
            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::BOOTROM_BASE, reinterpret_cast<uint8_t*>(&rdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE && rdata == 0x00000013,
                  "BootROM read via bus");

            uint32_t wdata = 0xBAADF00D;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::BOOTROM_BASE, reinterpret_cast<uint8_t*>(&wdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE,
                  "BootROM write rejection via bus");
        }

        // Address decode miss
        {
            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        0x50000000, reinterpret_cast<uint8_t*>(&rdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "Address decode miss (unmapped 0x50000000)");
        }

        // RAM DMI through bus
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, cfg::RAM_BASE, nullptr, 0);
            bool ok = bus_isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok && dmi_data.get_dmi_ptr() != nullptr, "RAM DMI via bus");
            check(dmi_data.get_start_address() == cfg::RAM_BASE, "RAM DMI start=global");
        }

        // BootROM DMI through bus
        {
            tlm::tlm_dmi dmi_data;
            setup_trans(trans, tlm::TLM_READ_COMMAND, cfg::BOOTROM_BASE, nullptr, 0);
            bool ok = bus_isock->get_direct_mem_ptr(trans, dmi_data);
            check(ok, "BootROM DMI via bus");
            check(dmi_data.get_start_address() == cfg::BOOTROM_BASE, "BootROM DMI start=global");
            check(dmi_data.is_read_allowed() && !dmi_data.is_write_allowed(),
                  "BootROM DMI read-only via bus");
        }

        // SRAM via bus
        {
            uint32_t wdata = 0x12345678;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::SRAM_BASE + 0x10, reinterpret_cast<uint8_t*>(&wdata), 4);
            bus_isock->b_transport(trans, delay);
            check(trans.get_response_status() == tlm::TLM_OK_RESPONSE, "SRAM write via bus");

            uint32_t rdata = 0;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::SRAM_BASE + 0x10, reinterpret_cast<uint8_t*>(&rdata), 4);
            bus_isock->b_transport(trans, delay);
            check(rdata == 0x12345678, "SRAM read-back via bus");
        }
    }

    void step3_rv32_defs() {
        std::cout << "\nStep 3: RV32 Definitions\n";
        using namespace rv32;

        check(imm_i(0x02A00293) == 42, "imm_i extracts 42");
        check(imm_i(0xFFF00293) == -1, "imm_i sign-extends -1");
        check(rd(0x02A00293) == 5, "rd extracts x5");
        check(rs1(0x002081B3) == 1, "rs1 extracts x1");
        check(rs2(0x002081B3) == 2, "rs2 extracts x2");
        check(opcode(0x02A00293) == OP_IMM, "opcode extracts OP_IMM");
        check(funct3(0x002081B3) == F3_ADD_SUB, "funct3 extracts ADD");
        check(funct7(0x402081B3) == F7_ALT, "funct7 extracts ALT (SUB)");
        check(imm_u(0x123450B7) == 0x12345000, "imm_u extracts upper");
        check(imm_b(0x00208463) == 8, "imm_b extracts +8");
    }

    void step4_decoder() {
        std::cout << "\nStep 4: Decoder\n";

        // LUI x1, 0x12345000
        {
            DecodedInstr d = decode(0x123450B7);
            check(d.type == InstrType::LUI, "LUI type");
            check(d.rd == 1, "LUI rd=x1");
            check(d.imm == 0x12345000, "LUI imm=0x12345000");
        }

        // ADDI x5, x0, 42
        {
            DecodedInstr d = decode(0x02A00293);
            check(d.type == InstrType::ADDI, "ADDI type");
            check(d.rd == 5 && d.rs1 == 0 && d.imm == 42, "ADDI operands");
        }

        // ADDI sign extension
        check(decode(0xFFF00293).imm == -1, "ADDI imm=-1 (sign extend)");

        // R-type
        {
            DecodedInstr d = decode(0x002081B3);
            check(d.type == InstrType::ADD, "ADD type");
            check(d.rd == 3 && d.rs1 == 1 && d.rs2 == 2, "ADD operands");
        }
        check(decode(0x402081B3).type == InstrType::SUB, "SUB type");

        // Branch
        {
            DecodedInstr d = decode(0x00208463);
            check(d.type == InstrType::BEQ, "BEQ type");
            check(d.rs1 == 1 && d.rs2 == 2 && d.imm == 8, "BEQ operands");
        }

        // Load/Store
        {
            DecodedInstr d = decode(0x0005A503);
            check(d.type == InstrType::LW, "LW type");
            check(d.rd == 10 && d.rs1 == 11 && d.imm == 0, "LW operands");
        }
        {
            DecodedInstr d = decode(0x00A5A223);
            check(d.type == InstrType::SW, "SW type");
            check(d.rs1 == 11 && d.rs2 == 10 && d.imm == 4, "SW operands");
        }

        // JAL/JALR
        {
            DecodedInstr d = decode(0x000000EF);
            check(d.type == InstrType::JAL, "JAL type");
            check(d.rd == 1 && d.imm == 0, "JAL operands");
        }
        {
            DecodedInstr d = decode(0x000280E7);
            check(d.type == InstrType::JALR, "JALR type");
            check(d.rd == 1 && d.rs1 == 5, "JALR operands");
        }

        // System
        check(decode(0x00000073).type == InstrType::ECALL,  "ECALL type");
        check(decode(0x00100073).type == InstrType::EBREAK, "EBREAK type");
        check(decode(0x30200073).type == InstrType::MRET,   "MRET type");

        // CSR
        {
            DecodedInstr d = decode(0x300110F3);
            check(d.type == InstrType::CSRRW, "CSRRW type");
            check(d.rd == 1 && d.rs1 == 2 && d.csr == 0x300, "CSRRW operands");
        }

        // M extension
        {
            DecodedInstr d = decode(0x022081B3);
            check(d.type == InstrType::MUL, "MUL type");
            check(d.rd == 3 && d.rs1 == 1 && d.rs2 == 2, "MUL operands");
        }
        check(decode(0x0220C1B3).type == InstrType::DIV, "DIV type");

        // A extension
        {
            DecodedInstr d = decode(0x1005A52F);
            check(d.type == InstrType::LR_W, "LR.W type");
            check(d.rd == 10 && d.rs1 == 11, "LR.W operands");
        }

        check(decode(0x0FF0000F).type == InstrType::FENCE, "FENCE type");

        // NOP
        {
            DecodedInstr d = decode(0x00000013);
            check(d.type == InstrType::ADDI, "NOP type");
            check(d.rd == 0 && d.rs1 == 0 && d.imm == 0, "NOP fields");
            check(!d.compressed && d.instr_len() == 4, "NOP 32-bit");
        }

        // Illegal
        check(decode(0x00000000).type == InstrType::ILLEGAL, "All-zeros is ILLEGAL");

        // Compressed
        {
            DecodedInstr d = decode(0x0001);
            check(d.type == InstrType::ADDI, "C.NOP type");
            check(d.compressed && d.instr_len() == 2, "C.NOP compressed/len");
        }
        {
            DecodedInstr d = decode(0x4515);
            check(d.type == InstrType::ADDI && d.rd == 10 && d.imm == 5, "C.LI x10,5");
            check(d.compressed, "C.LI compressed");
        }
        check(decode(0x856E).type == InstrType::ADD && decode(0x856E).compressed, "C.MV compressed");
        check(decode(0x9002).type == InstrType::EBREAK && decode(0x9002).compressed, "C.EBREAK compressed");
        {
            DecodedInstr d = decode(0x4502);
            check(d.type == InstrType::LW && d.rd == 10 && d.rs1 == 2, "C.LWSP x10,0(sp)");
            check(d.compressed, "C.LWSP compressed");
        }
    }

    void run_tests() {
        step1_memory();
        step2_bus();
        step3_rv32_defs();
        step4_decoder();
        sc_core::sc_stop();
    }
};

int sc_main(int, char*[])
{
    std::cout << "[VP] GamingCPU Virtual Platform -- Cumulative Tests\n";

    // Step 1: standalone memory instances (direct connection, no bus)
    Memory s1_ram("s1_ram", cfg::RAM_BASE, 0x100000);
    BootROM s1_rom("s1_rom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);
    uint32_t nop = 0x00000013;
    std::memcpy(s1_rom.data(), &nop, sizeof(nop));

    // Step 2: separate instances wired through the bus
    Memory ram("ram", cfg::RAM_BASE, 0x100000);
    Memory sram("sram", cfg::SRAM_BASE, cfg::SRAM_SIZE);
    BootROM bootrom("bootrom", cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);
    std::memcpy(bootrom.data(), &nop, sizeof(nop));

    TLM_Bus bus("bus");

    TestInitiator tester("tester");

    // Step 1 wiring: direct to standalone instances
    tester.mem_isock.bind(s1_ram.tsock);
    tester.rom_isock.bind(s1_rom.tsock);

    // Step 2 wiring: through bus
    tester.bus_isock.bind(bus.tsock);
    bus.isock.bind(bootrom.tsock);
    bus.map(cfg::BOOTROM_BASE, cfg::BOOTROM_SIZE);
    bus.isock.bind(sram.tsock);
    bus.map(cfg::SRAM_BASE, cfg::SRAM_SIZE);
    bus.isock.bind(ram.tsock);
    bus.map(cfg::RAM_BASE, 0x100000);

    sc_core::sc_start();

    std::cout << "\n=== All Results: " << pass_count << " passed, "
              << fail_count << " failed ===\n";

    if (fail_count > 0)
        SC_REPORT_FATAL("Test", "Some tests failed");

    return 0;
}
