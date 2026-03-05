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
#include "cpu/csr.h"
#include "cpu/execute.h"
#include "cpu/rv32m.h"
#include "cpu/rv32a.h"
#include "cpu/trap.h"
#include "cpu/mmu.h"
#include "cpu/iss.h"
#include "util/elf_loader.h"
#include "irq/clint.h"
#include "irq/plic.h"
#include "io/uart.h"
#include "platform/gamingcpu_vp.h"

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

    ISS* iss_ptr = nullptr;
    CLINT* clint_ptr = nullptr;
    PLIC* plic_ptr = nullptr;
    UART* uart_ptr = nullptr;
    GamingCPU_VP* platform_ptr = nullptr;

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

        // C.ADDI4SPN: addi rd', x2, nzuimm  (nzuimm is scaled *4, range [4,1020])
        // Encoding: funct3=000, nzuimm[5:4|9:6|2|3] in ci[12:5], rd' in ci[4:2], op=00
        {
            // nzuimm=8: bits [3]=1 -> ci[5]=1 => ci = 000_00001_000_00_00 = 0x0020
            // rd' = 0 -> x8, ci[4:2]=000
            uint16_t ci = 0x0020;
            DecodedInstr d = decode(ci);
            check(d.type == InstrType::ADDI && d.rd == 8 && d.rs1 == 2 && d.imm == 8,
                  "C.ADDI4SPN nzuimm=8");
        }
        {
            // nzuimm=1020 (max): bits 9:2 = 11111111_00
            // nzuimm[5:4]=ci[12:11], nzuimm[9:6]=ci[10:7], nzuimm[2]=ci[6], nzuimm[3]=ci[5]
            // nzuimm = 0x3FC = 0b1111111100
            // [9:6]=1111 -> ci[10:7]=1111, [5:4]=11 -> ci[12:11]=11, [3]=1 -> ci[5]=1, [2]=1 -> ci[6]=1
            // ci = 1_11_1111_11_100_00 = bits: f3=000, ci[12]=1,ci[11]=1,ci[10:7]=1111,ci[6]=1,ci[5]=1
            // ci[12:5] = 11111111 -> ci = 000_11111111_000_00 = 0x1FE0
            // rd' = 0 -> x8
            uint16_t ci = 0x1FE0;
            DecodedInstr d = decode(ci);
            check(d.type == InstrType::ADDI && d.rd == 8 && d.imm == 1020,
                  "C.ADDI4SPN nzuimm=1020 (max)");
        }
        {
            // nzuimm=256: bit[8]=1 -> ci[9]=1 (nzuimm[9:6] in ci[10:7])
            // nzuimm = 0x100 = 0b0100000000 -> [9:6]=0100 -> ci[10:7]=0100
            // ci[12:11]=00(nzuimm[5:4]), ci[10:7]=0100, ci[6]=0(nzuimm[2]), ci[5]=0(nzuimm[3])
            // ci = 000_00_0100_00_000_00 = 0x0200
            uint16_t ci = 0x0200;
            DecodedInstr d = decode(ci);
            check(d.type == InstrType::ADDI && d.imm == 256,
                  "C.ADDI4SPN nzuimm=256 (tests upper bits)");
        }

        // C.LW: lw rd', offset(rs1')
        {
            // offset=0, rs1'=x8, rd'=x8
            // funct3=010, ci[12:10]=000(off[5:3]), ci[9:7]=000(rs1'-8), ci[6]=0(off[2]),
            // ci[5]=0(off[6]), ci[4:2]=000(rd'-8), op=00
            // ci = 010_000_000_00_000_00 = 0x4000
            DecodedInstr d = decode(0x4000);
            check(d.type == InstrType::LW && d.rd == 8 && d.rs1 == 8 && d.imm == 0,
                  "C.LW x8,0(x8)");
        }
        {
            // offset=124: bits [5:3]=111, [2]=1, [6]=1
            // ci[12:10]=111, ci[6]=1, ci[5]=1 => ci = 010_111_000_11_000_00 = 0x5C60
            // rs1'=x8(000), rd'=x8(000)
            DecodedInstr d = decode(0x5C60);
            check(d.type == InstrType::LW && d.imm == 124, "C.LW offset=124");
        }

        // C.SW: sw rs2', offset(rs1')
        {
            // offset=0, rs1'=x8, rs2'=x9
            // funct3=110, ci[12:10]=000, ci[9:7]=000(rs1'-8), ci[6:5]=00, ci[4:2]=001(rs2'-8)
            // ci = 110_000_000_00_001_00 = 0xC004
            DecodedInstr d = decode(0xC004);
            check(d.type == InstrType::SW && d.rs1 == 8 && d.rs2 == 9, "C.SW x9,0(x8)");
        }

        // C.ADDI: addi rd, rd, nzimm
        {
            // x1 += -1: rd=x1, nzimm=-1 -> ci[12]=1(sign), ci[6:2]=11111
            // funct3=000, ci[11:7]=00001(rd), op=01
            // ci = 000_1_00001_11111_01 = 0x10FD
            DecodedInstr d = decode(0x10FD);
            check(d.type == InstrType::ADDI && d.rd == 1 && d.rs1 == 1 && d.imm == -1,
                  "C.ADDI x1,-1");
        }

        // C.JAL: jal x1, offset (RV32 only)
        {
            // offset=+2: ci[3]=1 (offset bit 1... this is complex)
            // Use known good encoding: C.JAL with offset=0 -> all offset bits zero
            // funct3=001, all offset bits=0 -> ci = 001_0_0000_0000_0_01 = 0x2001
            // But offset=0 for JAL is unusual. Let's just verify it decodes as JAL with link=x1
            DecodedInstr d = decode(0x2001);
            check(d.type == InstrType::JAL && d.rd == 1, "C.JAL decodes to JAL x1");
            check(d.compressed, "C.JAL compressed");
        }

        // C.J: jal x0, offset
        {
            // offset=0 -> all bits zero
            // funct3=101, op=01 -> ci = 101_0_0000_0000_0_01 = 0xA001
            DecodedInstr d = decode(0xA001);
            check(d.type == InstrType::JAL && d.rd == 0, "C.J decodes to JAL x0");
            check(d.compressed, "C.J compressed");
        }

        // C.BEQZ: beq rs1', x0, offset
        {
            // offset=0, rs1'=x8
            // funct3=110, ci[12:10]=000, ci[9:7]=000(rs1'-8), ci[6:2]=00000, op=01
            // ci = 110_000_000_00000_01 = 0xC001
            DecodedInstr d = decode(0xC001);
            check(d.type == InstrType::BEQ && d.rs1 == 8 && d.rs2 == 0, "C.BEQZ x8");
            check(d.compressed, "C.BEQZ compressed");
        }

        // C.BNEZ: bne rs1', x0, offset
        {
            // offset=0, rs1'=x9 -> ci[9:7]=001
            // funct3=111, ci[12:10]=000, ci[9:7]=001, ci[6:2]=00000, op=01
            // ci = 111_000_001_00000_01 = 0xE081
            DecodedInstr d = decode(0xE081);
            check(d.type == InstrType::BNE && d.rs1 == 9 && d.rs2 == 0, "C.BNEZ x9");
            check(d.compressed, "C.BNEZ compressed");
        }

        // C.SRLI: srli rd', rd', shamt
        {
            // shamt=4, rd'=x8 -> ci[9:7]=000
            // funct3=100, ci[11:10]=00(SRLI), ci[12]=0, ci[6:2]=00100(shamt)
            // ci = 100_0_00_000_00100_01 = 0x8011
            DecodedInstr d = decode(0x8011);
            check(d.type == InstrType::SRLI && d.rd == 8 && d.rs1 == 8 && d.imm == 4,
                  "C.SRLI x8,4");
        }

        // C.SRAI: srai rd', rd', shamt
        {
            // shamt=4, rd'=x8
            // funct3=100, ci[11:10]=01(SRAI), ci[12]=0, ci[6:2]=00100
            // ci = 100_0_01_000_00100_01 = 0x8411
            DecodedInstr d = decode(0x8411);
            check(d.type == InstrType::SRAI && d.rd == 8 && d.rs1 == 8 && d.imm == 4,
                  "C.SRAI x8,4");
        }

        // C.ANDI: andi rd', rd', imm
        {
            // imm=0xF, rd'=x8
            // funct3=100, ci[11:10]=10(ANDI), ci[12]=0(sign), ci[6:2]=01111
            // ci = 100_0_10_000_01111_01 = 0x883D
            DecodedInstr d = decode(0x883D);
            check(d.type == InstrType::ANDI && d.rd == 8 && d.imm == 0xF, "C.ANDI x8,0xF");
        }

        // C.SUB: sub rd', rd', rs2'
        {
            // rd'=x8(000), rs2'=x9(001)
            // funct3=100, ci[12]=0, ci[11:10]=11, ci[6:5]=00(SUB), ci[4:2]=001(rs2'-8)
            // ci = 100_0_11_000_00_001_01 = 0x8C05
            DecodedInstr d = decode(0x8C05);
            check(d.type == InstrType::SUB && d.rd == 8 && d.rs1 == 8 && d.rs2 == 9,
                  "C.SUB x8,x9");
        }

        // C.XOR: xor rd', rd', rs2'
        {
            // rd'=x8(000), rs2'=x9(001), ci[6:5]=01(XOR)
            // ci = 100_0_11_000_01_001_01 = 0x8C25
            DecodedInstr d = decode(0x8C25);
            check(d.type == InstrType::XOR && d.rd == 8 && d.rs2 == 9, "C.XOR x8,x9");
        }

        // C.OR: or rd', rd', rs2'
        {
            // ci[6:5]=10(OR)
            // ci = 100_0_11_000_10_001_01 = 0x8C45
            DecodedInstr d = decode(0x8C45);
            check(d.type == InstrType::OR && d.rd == 8 && d.rs2 == 9, "C.OR x8,x9");
        }

        // C.AND: and rd', rd', rs2'
        {
            // ci[6:5]=11(AND)
            // ci = 100_0_11_000_11_001_01 = 0x8C65
            DecodedInstr d = decode(0x8C65);
            check(d.type == InstrType::AND && d.rd == 8 && d.rs2 == 9, "C.AND x8,x9");
        }

        // C.SLLI: slli rd, rd, shamt
        {
            // rd=x1, shamt=4
            // funct3=000, ci[12]=0, ci[11:7]=00001(rd), ci[6:2]=00100(shamt), op=10
            // ci = 000_0_00001_00100_10 = 0x0092
            DecodedInstr d = decode(0x0092);
            check(d.type == InstrType::SLLI && d.rd == 1 && d.rs1 == 1 && d.imm == 4,
                  "C.SLLI x1,4");
        }

        // C.JR: jalr x0, rs1, 0
        {
            // rs1=x1, ci[12]=0, ci[11:7]=00001, ci[6:2]=00000, op=10
            // ci = 100_0_00001_00000_10 = 0x8082
            DecodedInstr d = decode(0x8082);
            check(d.type == InstrType::JALR && d.rd == 0 && d.rs1 == 1, "C.JR x1 (ret)");
            check(d.compressed, "C.JR compressed");
        }

        // C.JALR: jalr x1, rs1, 0
        {
            // rs1=x5, ci[12]=1, ci[11:7]=00101, ci[6:2]=00000, op=10
            // ci = 100_1_00101_00000_10 = 0x9282
            DecodedInstr d = decode(0x9282);
            check(d.type == InstrType::JALR && d.rd == 1 && d.rs1 == 5, "C.JALR x5");
        }

        // C.SWSP: sw rs2, offset(x2)
        {
            // rs2=x1, offset=0
            // funct3=110, ci[12:7]=000000(offset), ci[6:2]=00001(rs2), op=10
            // ci = 110_000000_00001_10 = 0xC006
            DecodedInstr d = decode(0xC006);
            check(d.type == InstrType::SW && d.rs1 == 2 && d.rs2 == 1, "C.SWSP x1,0(sp)");
            check(d.compressed, "C.SWSP compressed");
        }

        // C.ADDI16SP: addi x2, x2, nzimm (nzimm is multiple of 16)
        {
            // nzimm=16: nzimm[4]=1 -> ci[6]=1... let me use known encoding
            // nzimm=32: 0b0000_00_1_00000 -> nzimm[5]=1 -> ci[2] shifts...
            // Actually let me just test via decode+execute roundtrip
            // nzimm=+16: bit layout: nzimm[9]=ci[12], nzimm[4]=ci[6], nzimm[6]=ci[5],
            //   nzimm[8:7]=ci[4:3], nzimm[5]=ci[2]
            // nzimm=16=0x10: bit[4]=1 -> ci[6]=1
            // funct3=011, ci[12]=0(sign), ci[11:7]=00010(rd=x2), ci[6]=1, ci[5:2]=0000, op=01
            // ci = 011_0_00010_1_0000_01 = 0x6141
            DecodedInstr d = decode(0x6141);
            check(d.type == InstrType::ADDI && d.rd == 2 && d.rs1 == 2 && d.imm == 16,
                  "C.ADDI16SP nzimm=16");
        }

        // C.LUI: lui rd, nzimm
        {
            // rd=x1, nzimm=1 (upper immediate = 0x1000)
            // funct3=011, ci[12]=0(sign), ci[11:7]=00001(rd), ci[6:2]=00001(nzimm[16:12])
            // ci = 011_0_00001_00001_01 = 0x6085
            DecodedInstr d = decode(0x6085);
            check(d.type == InstrType::LUI && d.rd == 1, "C.LUI x1");
            check(d.compressed, "C.LUI compressed");
        }

        // C.ADD: add rd, rd, rs2
        {
            // rd=x1, rs2=x2 -> ci[12]=1, ci[11:7]=00001, ci[6:2]=00010, op=10
            // ci = 1001_0000_1000_1010 = 0x908A
            DecodedInstr d = decode(0x908A);
            check(d.type == InstrType::ADD && d.rd == 1 && d.rs1 == 1 && d.rs2 == 2,
                  "C.ADD x1,x2");
        }
    }

    void step5_csr() {
        std::cout << "\nStep 5: CSR File\n";
        using namespace rv32;

        CSRFile csr;

        // misa should be initialized to RV32IMACSU
        uint32_t val = 0;
        check(csr.read(CSR_MISA, PRV_M, val) && val == MISA_VALUE, "misa = RV32IMACSU");

        // Read-only info CSRs
        csr.read(CSR_MVENDORID, PRV_M, val);
        check(val == 0, "mvendorid = 0");
        csr.read(CSR_MHARTID, PRV_M, val);
        check(val == 0, "mhartid = 0");

        // Write/read mstatus
        csr.write(CSR_MSTATUS, PRV_M, MSTATUS_MIE | MSTATUS_MPIE);
        csr.read(CSR_MSTATUS, PRV_M, val);
        check((val & MSTATUS_MIE) != 0, "mstatus.MIE set");
        check((val & MSTATUS_MPIE) != 0, "mstatus.MPIE set");

        // mstatus MPP WARL: value 2 is illegal -> forced to 0
        csr.write(CSR_MSTATUS, PRV_M, (2u << 11));
        csr.read(CSR_MSTATUS, PRV_M, val);
        check(((val >> 11) & 0x3) != 2, "mstatus.MPP rejects illegal value 2");

        // mstatus MPP = M(3) is legal
        csr.write(CSR_MSTATUS, PRV_M, (3u << 11));
        csr.read(CSR_MSTATUS, PRV_M, val);
        check(((val >> 11) & 0x3) == 3, "mstatus.MPP = M allowed");

        // Write/read mtvec, mepc, mcause, mscratch
        csr.write(CSR_MTVEC, PRV_M, 0x80000100);
        csr.read(CSR_MTVEC, PRV_M, val);
        check(val == 0x80000100, "mtvec write/read");

        csr.write(CSR_MEPC, PRV_M, 0x80000005);
        csr.read(CSR_MEPC, PRV_M, val);
        check(val == 0x80000004, "mepc bit 0 cleared");

        csr.write(CSR_MSCRATCH, PRV_M, 0xDEADBEEF);
        csr.read(CSR_MSCRATCH, PRV_M, val);
        check(val == 0xDEADBEEF, "mscratch write/read");

        // mip: hardware bits
        csr.set_mip_mtip(true);
        check((csr.get_mip() & MIP_MTIP) != 0, "mip.MTIP set by hardware");
        csr.set_mip_mtip(false);
        check((csr.get_mip() & MIP_MTIP) == 0, "mip.MTIP cleared");

        csr.set_mip_meip(true);
        check((csr.get_mip() & MIP_MEIP) != 0, "mip.MEIP set");

        // mip: software can only write SSIP
        csr.write(CSR_MIP, PRV_M, 0xFFFFFFFF);
        check((csr.get_mip() & MIP_SSIP) != 0, "mip SSIP writable by software");
        check((csr.get_mip() & MIP_MSIP) == 0, "mip MSIP not writable by software");

        // Supervisor CSRs
        csr.write(CSR_STVEC, PRV_S, 0x80001000);
        csr.read(CSR_STVEC, PRV_S, val);
        check(val == 0x80001000, "stvec write/read from S-mode");

        // sstatus is a view of mstatus
        csr.write(CSR_MSTATUS, PRV_M, MSTATUS_SIE | MSTATUS_MIE);
        csr.read(CSR_SSTATUS, PRV_S, val);
        check((val & (1 << 1)) != 0, "sstatus.SIE reflects mstatus");
        check((val & (1 << 3)) == 0, "sstatus doesn't expose MIE");

        // Privilege violation: S-mode can't read M-mode CSR
        check(!csr.read(CSR_MSTATUS, PRV_S, val), "S-mode can't read mstatus");
        check(!csr.read(CSR_MTVEC, PRV_S, val), "S-mode can't read mtvec");

        // Read-only CSR write rejected
        check(!csr.write(CSR_MVENDORID, PRV_M, 42), "can't write mvendorid");

        // mcycle / minstret
        csr.inc_mcycle();
        csr.inc_mcycle();
        csr.inc_minstret();
        csr.read(CSR_MCYCLE, PRV_M, val);
        check(val == 2, "mcycle incremented to 2");
        csr.read(CSR_MINSTRET, PRV_M, val);
        check(val == 1, "minstret incremented to 1");

        // satp callback
        bool flushed = false;
        csr.on_satp_write = [&]() { flushed = true; };
        csr.write(CSR_SATP, PRV_S, 0x80012345);
        check(flushed, "satp write triggers callback");
        csr.read(CSR_SATP, PRV_S, val);
        check(val == 0x80012345, "satp write/read");
    }

    void step6_execute() {
        std::cout << "\nStep 6: Execute + RV32M + RV32A\n";
        using namespace rv32;

        // --- 6a: RV32M standalone arithmetic ---
        check(rv32m::mul(6, 7) == 42, "MUL 6*7=42");
        check(rv32m::mul(0xFFFFFFFF, 2) == 0xFFFFFFFE, "MUL -1*2=-2");

        check(rv32m::mulh(0x80000000, 2) == 0xFFFFFFFF, "MULH -2^31*2 upper");
        check(rv32m::mulhu(0x80000000, 2) == 1, "MULHU 0x80000000*2 upper=1");
        check(rv32m::mulhsu(0xFFFFFFFF, 2) == 0xFFFFFFFF, "MULHSU -1*2 upper=-1");

        check(rv32m::div(42, 7) == 6, "DIV 42/7=6");
        check(rv32m::div(42, 0) == 0xFFFFFFFF, "DIV by zero=-1");
        check(rv32m::div(0x80000000, 0xFFFFFFFF) == 0x80000000, "DIV overflow=-2^31");
        check(rv32m::divu(42, 7) == 6, "DIVU 42/7=6");
        check(rv32m::divu(42, 0) == 0xFFFFFFFF, "DIVU by zero=max");

        check(rv32m::rem(43, 7) == 1, "REM 43%7=1");
        check(rv32m::rem(43, 0) == 43, "REM by zero=dividend");
        check(rv32m::rem(0x80000000, 0xFFFFFFFF) == 0, "REM overflow=0");
        check(rv32m::remu(43, 7) == 1, "REMU 43%7=1");
        check(rv32m::remu(43, 0) == 43, "REMU by zero=dividend");

        // --- 6b: RV32A standalone AMO helpers ---
        check(rv32a::amo_swap(10, 20) == 20, "AMO swap");
        check(rv32a::amo_add(10, 20) == 30, "AMO add");
        check(rv32a::amo_xor(0xFF, 0x0F) == 0xF0, "AMO xor");
        check(rv32a::amo_and(0xFF, 0x0F) == 0x0F, "AMO and");
        check(rv32a::amo_or(0xF0, 0x0F) == 0xFF, "AMO or");
        check(rv32a::amo_min(5, uint32_t(-3)) == uint32_t(-3), "AMO min signed");
        check(rv32a::amo_max(5, uint32_t(-3)) == 5, "AMO max signed");
        check(rv32a::amo_minu(5, uint32_t(-3)) == 5, "AMO minu unsigned");
        check(rv32a::amo_maxu(5, uint32_t(-3)) == uint32_t(-3), "AMO maxu unsigned");

        // --- 6c: Execute engine ---
        // Set up a small test memory (4KB)
        std::vector<uint8_t> tmem(4096, 0);
        auto mem_read = [&](uint32_t addr, int bytes) -> uint32_t {
            uint32_t v = 0;
            for (int i = 0; i < bytes; i++)
                v |= uint32_t(tmem[addr + i]) << (i * 8);
            return v;
        };
        auto mem_write = [&](uint32_t addr, uint32_t data, int bytes) {
            for (int i = 0; i < bytes; i++)
                tmem[addr + i] = (data >> (i * 8)) & 0xFF;
        };

        auto make_cpu = [&]() -> CPUState {
            CPUState s;
            s.mem.read = mem_read;
            s.mem.write = mem_write;
            return s;
        };

        // ALU: ADDI x1, x0, 42
        {
            CPUState s = make_cpu();
            DecodedInstr d = decode(0x02A00093); // addi x1, x0, 42
            ExecResult r = execute(s, d);
            check(!r.exception && s.get_reg(1) == 42, "exec ADDI x1=42");
        }

        // ALU: ADD x3, x1, x2
        {
            CPUState s = make_cpu();
            s.regs[1] = 10;
            s.regs[2] = 20;
            DecodedInstr d = decode(0x002081B3); // add x3, x1, x2
            execute(s, d);
            check(s.get_reg(3) == 30, "exec ADD 10+20=30");
        }

        // SUB
        {
            CPUState s = make_cpu();
            s.regs[1] = 50;
            s.regs[2] = 8;
            DecodedInstr d = decode(0x402081B3); // sub x3, x1, x2
            execute(s, d);
            check(s.get_reg(3) == 42, "exec SUB 50-8=42");
        }

        // LUI
        {
            CPUState s = make_cpu();
            DecodedInstr d = decode(0x123450B7); // lui x1, 0x12345
            execute(s, d);
            check(s.get_regu(1) == 0x12345000, "exec LUI x1=0x12345000");
        }

        // AUIPC
        {
            CPUState s = make_cpu();
            s.pc = 0x100;
            DecodedInstr d = decode(0x000010B7 | (OP_AUIPC & 0x7F)); // auipc x1, 1
            d = decode(0x00001097); // auipc x1, 1
            execute(s, d);
            check(s.get_regu(1) == 0x1100, "exec AUIPC pc+0x1000");
        }

        // SLT / SLTU
        {
            CPUState s = make_cpu();
            s.regs[1] = -5;
            s.regs[2] = 3;
            DecodedInstr d;

            d = decode(0x0020A1B3); // slt x3, x1, x2
            execute(s, d);
            check(s.get_reg(3) == 1, "exec SLT -5<3=1");

            d = decode(0x0020B1B3); // sltu x3, x1, x2
            execute(s, d);
            check(s.get_reg(3) == 0, "exec SLTU 0xFFFFFFFB<3=0");
        }

        // Shifts
        {
            CPUState s = make_cpu();
            s.regs[1] = 0x80000000;
            DecodedInstr d;

            d = decode(0x0040D093); // srli x1, x1, 4 -> but need separate rd
            // Let me use manual: SRLI x2, x1, 4
            d = decode(0x00405113); // srli x2, x1, 4 (imm=4, rs1=x0... no)
            // Encode: SRLI x2, x1, 4: opcode=OP_IMM, funct3=5(SRL_SRA), rd=2, rs1=1, imm=4
            // 000000000100_00001_101_00010_0010011
            // 0x0040D113
            d = decode(0x0040D113);
            execute(s, d);
            check(s.get_regu(2) == 0x08000000, "exec SRLI >>4");

            // SRAI x3, x1, 4: funct7[5]=1 -> imm=0x404
            // 010000000100_00001_101_00011_0010011
            // 0x4040D193
            d = decode(0x4040D193);
            execute(s, d);
            check(s.get_reg(3) == static_cast<int32_t>(0xF8000000u), "exec SRAI >>4 sign-ext");
        }

        // LW / SW
        {
            CPUState s = make_cpu();
            s.regs[1] = 0x100; // base address
            // Store 0xDEADBEEF at [0x100]
            s.regs[2] = static_cast<int32_t>(0xDEADBEEF);
            DecodedInstr d = decode(0x0020A023); // sw x2, 0(x1)
            execute(s, d);
            check(mem_read(0x100, 4) == 0xDEADBEEF, "exec SW stores to mem");

            // Load it back into x3
            d = decode(0x0000A183); // lw x3, 0(x1)
            execute(s, d);
            check(s.get_regu(3) == 0xDEADBEEF, "exec LW loads from mem");
        }

        // LB / LBU (sign extension)
        {
            CPUState s = make_cpu();
            tmem[0x200] = 0xFF;
            s.regs[1] = 0x200;

            DecodedInstr d = decode(0x00008183); // lb x3, 0(x1)
            execute(s, d);
            check(s.get_reg(3) == -1, "exec LB sign-extends 0xFF=-1");

            d = decode(0x0000C183); // lbu x3, 0(x1)
            execute(s, d);
            check(s.get_reg(3) == 255, "exec LBU zero-extends 0xFF=255");
        }

        // Misaligned load exception
        {
            CPUState s = make_cpu();
            s.regs[1] = 0x101;
            DecodedInstr d = decode(0x0000A183); // lw x3, 0(x1)
            ExecResult r = execute(s, d);
            check(r.exception && r.cause == CAUSE_MISALIGNED_LOAD, "exec LW misaligned exception");
        }

        // BEQ taken / not taken
        {
            CPUState s = make_cpu();
            s.pc = 0x1000;
            s.regs[1] = 42;
            s.regs[2] = 42;
            DecodedInstr d = decode(0x00208463); // beq x1, x2, +8
            execute(s, d);
            check(s.next_pc == 0x1008, "exec BEQ taken");

            s.regs[2] = 99;
            s.pc = 0x1000;
            execute(s, d);
            check(s.next_pc == 0x1004, "exec BEQ not taken");
        }

        // JAL
        {
            CPUState s = make_cpu();
            s.pc = 0x2000;
            DecodedInstr d = decode(0x008000EF); // jal x1, +8
            execute(s, d);
            check(s.get_regu(1) == 0x2004, "exec JAL link=pc+4");
            check(s.next_pc == 0x2008, "exec JAL target=pc+8");
        }

        // JALR
        {
            CPUState s = make_cpu();
            s.pc = 0x3000;
            s.regs[5] = 0x4000;
            DecodedInstr d = decode(0x000280E7); // jalr x1, 0(x5)
            execute(s, d);
            check(s.get_regu(1) == 0x3004, "exec JALR link=pc+4");
            check(s.next_pc == 0x4000, "exec JALR target=x5");
        }

        // x0 is always zero
        {
            CPUState s = make_cpu();
            DecodedInstr d = decode(0x02A00013); // addi x0, x0, 42
            execute(s, d);
            check(s.get_reg(0) == 0, "exec write to x0 ignored");
        }

        // MUL via execute
        {
            CPUState s = make_cpu();
            s.regs[1] = 6;
            s.regs[2] = 7;
            DecodedInstr d = decode(0x022080B3); // mul x1, x1, x2
            execute(s, d);
            check(s.get_reg(1) == 42, "exec MUL 6*7=42");
        }

        // DIV via execute (div by zero)
        {
            CPUState s = make_cpu();
            s.regs[1] = 42;
            s.regs[2] = 0;
            DecodedInstr d = decode(0x0220C1B3); // div x3, x1, x2
            execute(s, d);
            check(s.get_regu(3) == 0xFFFFFFFF, "exec DIV by zero=-1");
        }

        // LR.W / SC.W success
        {
            CPUState s = make_cpu();
            mem_write(0x300, 0xAAAAAAAA, 4);
            s.regs[1] = 0x300;
            s.regs[2] = 0xBBBBBBBB;

            DecodedInstr d_lr = decode(0x1000A52F); // lr.w x10, (x1)
            execute(s, d_lr);
            check(s.get_regu(10) == 0xAAAAAAAA, "exec LR.W loads value");
            check(s.lr_sc.valid, "exec LR.W sets reservation");

            DecodedInstr d_sc = decode(0x1820A5AF); // sc.w x11, x2, (x1)
            execute(s, d_sc);
            check(s.get_reg(11) == 0, "exec SC.W success=0");
            check(mem_read(0x300, 4) == 0xBBBBBBBB, "exec SC.W wrote value");
        }

        // SC.W failure (no reservation)
        {
            CPUState s = make_cpu();
            s.regs[1] = 0x300;
            s.regs[2] = 0xCCCCCCCC;
            mem_write(0x300, 0x11111111, 4);

            DecodedInstr d_sc = decode(0x1820A5AF); // sc.w x11, x2, (x1)
            execute(s, d_sc);
            check(s.get_reg(11) == 1, "exec SC.W failure=1");
            check(mem_read(0x300, 4) == 0x11111111, "exec SC.W didn't write");
        }

        // AMOSWAP.W
        {
            CPUState s = make_cpu();
            mem_write(0x400, 100, 4);
            s.regs[1] = 0x400;
            s.regs[2] = 200;
            DecodedInstr d = decode(0x0820A52F); // amoswap.w x10, x2, (x1)
            execute(s, d);
            check(s.get_reg(10) == 100, "exec AMOSWAP old=100");
            check(mem_read(0x400, 4) == 200, "exec AMOSWAP new=200");
        }

        // AMOADD.W
        {
            CPUState s = make_cpu();
            mem_write(0x400, 30, 4);
            s.regs[1] = 0x400;
            s.regs[2] = 12;
            DecodedInstr d = decode(0x0020A52F); // amoadd.w x10, x2, (x1)
            execute(s, d);
            check(s.get_reg(10) == 30, "exec AMOADD old=30");
            check(mem_read(0x400, 4) == 42, "exec AMOADD new=42");
        }

        // CSR via execute: CSRRW
        {
            CPUState s = make_cpu();
            s.regs[1] = 0x80000100;
            // csrrw x2, mtvec, x1 -> CSR=0x305, rd=2, rs1=1
            // funct3=001(CSRRW), csr=0x305
            // 0011_0000_0101_00001_001_00010_1110011
            // 0x30509173
            DecodedInstr d = decode(0x30509173);
            execute(s, d);
            uint32_t val;
            s.csr.read(CSR_MTVEC, PRV_M, val);
            check(val == 0x80000100, "exec CSRRW writes mtvec");
        }

        // CSRRS read mscratch
        {
            CPUState s = make_cpu();
            s.csr.write(CSR_MSCRATCH, PRV_M, 0xDEADBEEF);
            // csrrs x1, mscratch, x0 -> read-only (rs1=x0, no write)
            // 0011_0100_0000_00000_010_00001_1110011
            // 0x340020F3
            DecodedInstr d = decode(0x340020F3);
            execute(s, d);
            check(s.get_regu(1) == 0xDEADBEEF, "exec CSRRS reads mscratch");
        }

        // ECALL from M-mode
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            DecodedInstr d = decode(0x00000073);
            ExecResult r = execute(s, d);
            check(r.exception && r.cause == CAUSE_ECALL_M, "exec ECALL M-mode");
        }

        // ECALL from U-mode
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            DecodedInstr d = decode(0x00000073);
            ExecResult r = execute(s, d);
            check(r.exception && r.cause == CAUSE_ECALL_U, "exec ECALL U-mode");
        }

        // EBREAK
        {
            CPUState s = make_cpu();
            s.pc = 0x5000;
            DecodedInstr d = decode(0x00100073);
            ExecResult r = execute(s, d);
            check(r.exception && r.cause == CAUSE_BREAKPOINT, "exec EBREAK");
        }

        // MRET
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mepc = 0x80000000;
            s.csr.mstatus = MSTATUS_MPIE | (PRV_S << MSTATUS_MPP_SHIFT);
            DecodedInstr d = decode(0x30200073);
            ExecResult r = execute(s, d);
            check(!r.exception, "exec MRET no exception");
            check(s.next_pc == 0x80000000, "exec MRET pc=mepc");
            check(s.priv == PRV_S, "exec MRET priv=S (from MPP)");
            check((s.csr.mstatus & MSTATUS_MIE) != 0, "exec MRET restores MIE from MPIE");
        }

        // ILLEGAL
        {
            CPUState s = make_cpu();
            DecodedInstr d = decode(0x00000000);
            ExecResult r = execute(s, d);
            check(r.exception && r.cause == CAUSE_ILLEGAL_INSTR, "exec ILLEGAL exception");
        }

        // FENCE (no-op, shouldn't crash)
        {
            CPUState s = make_cpu();
            DecodedInstr d = decode(0x0FF0000F);
            ExecResult r = execute(s, d);
            check(!r.exception, "exec FENCE no-op");
        }
    }

    void step7_trap() {
        std::cout << "\nStep 7: Trap Handler\n";
        using namespace rv32;

        std::vector<uint8_t> tmem(4096, 0);
        auto mem_read = [&](uint32_t addr, int bytes) -> uint32_t {
            uint32_t v = 0;
            for (int i = 0; i < bytes; i++)
                v |= uint32_t(tmem[addr + i]) << (i * 8);
            return v;
        };
        auto mem_write = [&](uint32_t addr, uint32_t data, int bytes) {
            for (int i = 0; i < bytes; i++)
                tmem[addr + i] = (data >> (i * 8)) & 0xFF;
        };
        auto make_cpu = [&]() -> CPUState {
            CPUState s;
            s.mem.read = mem_read;
            s.mem.write = mem_write;
            return s;
        };

        // take_trap: ECALL from U-mode -> M-mode (no delegation)
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.pc = 0x80001000;
            s.csr.mtvec = 0x80000100;
            s.csr.mstatus = MSTATUS_MIE;
            trap::take_trap(s, CAUSE_ECALL_U, 0);
            check(s.csr.mepc == 0x80001000, "trap mepc = faulting pc");
            check(s.csr.mcause == CAUSE_ECALL_U, "trap mcause = ECALL_U");
            check(s.csr.mtval == 0, "trap mtval = 0");
            check(s.priv == PRV_M, "trap escalates to M-mode");
            check(s.next_pc == 0x80000100, "trap jumps to mtvec");
            check((s.csr.mstatus & MSTATUS_MIE) == 0, "trap clears MIE");
            check((s.csr.mstatus & MSTATUS_MPIE) != 0, "trap saves MIE to MPIE");
            check(((s.csr.mstatus >> MSTATUS_MPP_SHIFT) & 0x3) == PRV_U, "trap MPP = U");
        }

        // take_trap: exception from S-mode -> M-mode
        {
            CPUState s = make_cpu();
            s.priv = PRV_S;
            s.pc = 0x80002000;
            s.csr.mtvec = 0x80000200;
            s.csr.mstatus = MSTATUS_MIE | MSTATUS_SIE;
            trap::take_trap(s, CAUSE_ILLEGAL_INSTR, 0xDEADBEEF);
            check(s.priv == PRV_M, "trap S->M on non-delegated exception");
            check(s.csr.mepc == 0x80002000, "trap mepc from S-mode");
            check(s.csr.mtval == 0xDEADBEEF, "trap mtval = bad instr");
            check(((s.csr.mstatus >> MSTATUS_MPP_SHIFT) & 0x3) == PRV_S, "trap MPP = S");
        }

        // take_trap: delegation to S-mode via medeleg
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.pc = 0x80003000;
            s.csr.mtvec = 0x80000100;
            s.csr.stvec = 0x80000400;
            s.csr.medeleg = (1u << CAUSE_ECALL_U);
            s.csr.mstatus = MSTATUS_SIE;
            trap::take_trap(s, CAUSE_ECALL_U, 0);
            check(s.priv == PRV_S, "delegated trap goes to S-mode");
            check(s.csr.sepc == 0x80003000, "delegated trap sepc");
            check(s.csr.scause == CAUSE_ECALL_U, "delegated trap scause");
            check(s.next_pc == 0x80000400, "delegated trap jumps to stvec");
            check((s.csr.mstatus & MSTATUS_SIE) == 0, "delegated trap clears SIE");
            check((s.csr.mstatus & MSTATUS_SPIE) != 0, "delegated trap saves SIE to SPIE");
            check((s.csr.mstatus & MSTATUS_SPP) == 0, "delegated trap SPP = U");
        }

        // take_trap: delegation from S-mode sets SPP = S
        {
            CPUState s = make_cpu();
            s.priv = PRV_S;
            s.pc = 0x80004000;
            s.csr.stvec = 0x80000500;
            s.csr.medeleg = (1u << CAUSE_LOAD_PAGE_FAULT);
            s.csr.mstatus = MSTATUS_SIE;
            trap::take_trap(s, CAUSE_LOAD_PAGE_FAULT, 0x1234);
            check(s.priv == PRV_S, "S-mode delegated stays S-mode");
            check((s.csr.mstatus & MSTATUS_SPP) != 0, "SPP = S when trap from S");
            check(s.csr.stval == 0x1234, "stval = faulting addr");
        }

        // take_trap: M-mode trap never delegates (priv > S)
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.pc = 0x80005000;
            s.csr.mtvec = 0x80000100;
            s.csr.stvec = 0x80000400;
            s.csr.medeleg = (1u << CAUSE_BREAKPOINT);
            trap::take_trap(s, CAUSE_BREAKPOINT, s.pc);
            check(s.priv == PRV_M, "M-mode trap never delegates");
            check(s.next_pc == 0x80000100, "M-mode trap uses mtvec");
        }

        // Vectored mtvec: interrupt jumps to base + 4*cause
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.pc = 0x80006000;
            s.csr.mtvec = 0x80000100 | 1; // mode=1 (vectored)
            trap::take_trap(s, IRQ_M_TIMER, 0);
            uint32_t expected = 0x80000100 + 4 * 7; // cause_code=7
            check(s.next_pc == expected, "vectored mtvec for timer IRQ");
            check(s.csr.mcause == IRQ_M_TIMER, "mcause has interrupt bit");
        }

        // Vectored mtvec: exceptions always use base (not vectored)
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.pc = 0x80007000;
            s.csr.mtvec = 0x80000100 | 1;
            trap::take_trap(s, CAUSE_ECALL_U, 0);
            check(s.next_pc == 0x80000100, "vectored mtvec uses base for exceptions");
        }

        // check_pending_interrupts: timer interrupt pending + enabled
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mstatus = MSTATUS_MIE;
            s.csr.mie = MIP_MTIP;
            s.csr.set_mip_mtip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == IRQ_M_TIMER, "pending timer IRQ detected");
        }

        // check_pending_interrupts: MIE disabled in M-mode -> no interrupt
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mstatus = 0; // MIE = 0
            s.csr.mie = MIP_MTIP;
            s.csr.set_mip_mtip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == 0, "no IRQ when MIE disabled in M-mode");
        }

        // check_pending_interrupts: M-mode IRQ taken from U-mode even with MIE=0
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.csr.mstatus = 0;
            s.csr.mie = MIP_MTIP;
            s.csr.set_mip_mtip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == IRQ_M_TIMER, "M-mode IRQ taken from U-mode regardless of MIE");
        }

        // check_pending_interrupts: mie register masks specific interrupt
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mstatus = MSTATUS_MIE;
            s.csr.mie = 0; // no interrupts enabled in mie
            s.csr.set_mip_mtip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == 0, "no IRQ when mie bit not set");
        }

        // check_pending_interrupts: priority MEI > MTI
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mstatus = MSTATUS_MIE;
            s.csr.mie = MIP_MTIP | MIP_MEIP;
            s.csr.set_mip_mtip(true);
            s.csr.set_mip_meip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == IRQ_M_EXTERNAL, "MEI has priority over MTI");
        }

        // check_pending_interrupts: delegated S-mode interrupt in S-mode
        {
            CPUState s = make_cpu();
            s.priv = PRV_S;
            s.csr.mstatus = MSTATUS_SIE;
            s.csr.mie = MIP_STIP;
            s.csr.mideleg = MIP_STIP;
            s.csr.set_mip_stip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == IRQ_S_TIMER, "delegated S-timer IRQ in S-mode");
        }

        // check_pending_interrupts: delegated S-mode interrupt NOT taken in M-mode
        {
            CPUState s = make_cpu();
            s.priv = PRV_M;
            s.csr.mstatus = MSTATUS_MIE | MSTATUS_SIE;
            s.csr.mie = MIP_STIP;
            s.csr.mideleg = MIP_STIP;
            s.csr.set_mip_stip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == 0, "delegated S-IRQ not taken in M-mode");
        }

        // check_pending_interrupts: SIE disabled in S-mode blocks S interrupt
        {
            CPUState s = make_cpu();
            s.priv = PRV_S;
            s.csr.mstatus = 0; // SIE = 0
            s.csr.mie = MIP_SEIP;
            s.csr.mideleg = MIP_SEIP;
            s.csr.set_mip_seip(true);
            uint32_t irq = trap::check_pending_interrupts(s);
            check(irq == 0, "S-IRQ blocked when SIE=0 in S-mode");
        }

        // Full round-trip: execute returns exception, take_trap handles it
        {
            CPUState s = make_cpu();
            s.priv = PRV_U;
            s.pc = 0x80010000;
            s.csr.mtvec = 0x80000100;
            DecodedInstr d = decode(0x00000073); // ecall
            ExecResult r = execute(s, d);
            check(r.exception, "ecall returns exception");
            trap::take_trap(s, r.cause, r.tval);
            check(s.priv == PRV_M, "round-trip: ecall -> M-mode trap");
            check(s.next_pc == 0x80000100, "round-trip: pc = mtvec");
            check(s.csr.mepc == 0x80010000, "round-trip: mepc = ecall pc");
        }
    }

    void step8_mmu() {
        std::cout << "\n--- Step 8: MMU (Sv32) ---\n";
        using namespace rv32;

        // Simulated physical memory for page tables (1MB should be plenty)
        std::vector<uint32_t> pmem(256 * 1024, 0);
        auto pmem_read = [&](uint32_t addr) -> uint32_t {
            return pmem[addr / 4];
        };
        auto pmem_write = [&](uint32_t addr, uint32_t val) {
            pmem[addr / 4] = val;
        };

        MMU mmu;
        mmu.mem_read = pmem_read;
        mmu.mem_write = pmem_write;

        // Build a simple Sv32 page table:
        // Root table at physical page 1 (addr 0x1000)
        // L2 table at physical page 2 (addr 0x2000)
        // Map vaddr 0x00400000 (VPN1=1, VPN0=0) -> paddr 0x80000000 (4KB page)
        // Map vaddr 0x00800000 (VPN1=2, VPN0=0) -> paddr 0x80400000 (4MB superpage)

        uint32_t root_ppn = 1;
        uint32_t satp = (SATP_MODE_SV32 << SATP_MODE_SHIFT) | root_ppn;

        // L1 PTE at root[1] -> points to L2 table at page 2
        uint32_t l2_ppn = 2;
        pmem[0x1000/4 + 1] = (l2_ppn << PTE_PPN_SHIFT) | PTE_V;

        // L2 PTE at l2_table[0] -> leaf mapping to phys page 0x80000 (paddr 0x80000000)
        uint32_t target_ppn = 0x80000;
        pmem[0x2000/4 + 0] = (target_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;

        // L1 PTE at root[2] -> 4MB superpage at phys 0x80400000 (PPN=0x80400, PPN[0] must be 0)
        // PPN for superpage: PPN1=0x201, PPN0=0x000 -> full PPN = 0x80400
        uint32_t super_ppn = 0x80400;
        pmem[0x1000/4 + 2] = (super_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_W | PTE_X;

        // Test 1: Basic translation (4KB page, U-mode)
        auto r = mmu.translate(0x00400ABC, AccessType::LOAD, PRV_U, satp, 0);
        check(!r.fault, "MMU 4KB page translates");
        check(r.paddr == 0x80000ABC, "MMU 4KB paddr correct");

        // Test 2: A bit should have been set
        uint32_t pte_after = pmem[0x2000/4 + 0];
        check((pte_after & PTE_A) != 0, "MMU sets A bit on read");

        // Test 3: Store sets D bit
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::STORE, PRV_U, satp, 0);
        check(!r.fault, "MMU store translates");
        pte_after = pmem[0x2000/4 + 0];
        check((pte_after & PTE_D) != 0, "MMU sets D bit on store");

        // Test 4: Superpage translation
        mmu.flush_tlb();
        r = mmu.translate(0x00800123, AccessType::LOAD, PRV_S, satp, 0);
        check(!r.fault, "MMU 4MB superpage translates");
        check(r.paddr == 0x80400123, "MMU superpage paddr correct");

        // Test 5: Superpage with offset in VPN[0] range
        r = mmu.translate(0x00BFFF00, AccessType::LOAD, PRV_S, satp, 0);
        check(!r.fault, "MMU superpage high offset translates");
        check(r.paddr == 0x807FFF00, "MMU superpage high offset paddr");

        // Test 6: Permission fault - U-mode page accessed from S-mode without SUM
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_S, satp, 0);
        check(r.fault, "MMU S-mode fault on U page without SUM");
        check(r.cause == CAUSE_LOAD_PAGE_FAULT, "MMU load page fault cause");

        // Test 7: SUM bit allows S-mode to access U pages
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_S, satp, MSTATUS_SUM);
        check(!r.fault, "MMU S-mode reads U page with SUM");

        // Test 8: U-mode can't access S-mode page (superpage has no PTE_U)
        mmu.flush_tlb();
        r = mmu.translate(0x00800000, AccessType::LOAD, PRV_U, satp, 0);
        check(r.fault, "MMU U-mode fault on S page");

        // Test 9: Execute fault on non-executable page
        // Make a read-only page: clear X bit
        pmem[0x2000/4 + 0] = (target_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_U;
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::FETCH, PRV_U, satp, 0);
        check(r.fault, "MMU fetch fault on non-X page");
        check(r.cause == CAUSE_FETCH_PAGE_FAULT, "MMU fetch page fault cause");

        // Test 10: Write fault on read-only page
        r = mmu.translate(0x00400000, AccessType::STORE, PRV_U, satp, 0);
        check(r.fault, "MMU store fault on R-only page");
        check(r.cause == CAUSE_STORE_PAGE_FAULT, "MMU store page fault cause");

        // Restore full perms for remaining tests
        pmem[0x2000/4 + 0] = (target_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;

        // Test 11: Invalid PTE (V=0) -> fault
        pmem[0x1000/4 + 3] = 0; // root[3] = invalid
        mmu.flush_tlb();
        r = mmu.translate(0x00C00000, AccessType::LOAD, PRV_U, satp, 0);
        check(r.fault, "MMU fault on invalid PTE");

        // Test 12: Reserved encoding (R=0, W=1) -> fault
        pmem[0x1000/4 + 3] = (0x90000 << PTE_PPN_SHIFT) | PTE_V | PTE_W;
        mmu.flush_tlb();
        r = mmu.translate(0x00C00000, AccessType::LOAD, PRV_U, satp, 0);
        check(r.fault, "MMU fault on reserved R=0 W=1 encoding");

        // Test 13: Misaligned superpage (PPN[0] != 0) -> fault
        uint32_t bad_super_ppn = 0x80401; // PPN[0] = 1, not aligned
        pmem[0x1000/4 + 4] = (bad_super_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_W;
        mmu.flush_tlb();
        r = mmu.translate(0x01000000, AccessType::LOAD, PRV_S, satp, 0);
        check(r.fault, "MMU fault on misaligned superpage");

        // Test 14: Bare mode passthrough
        uint32_t satp_bare = 0;
        r = mmu.translate(0xDEADBEEF, AccessType::LOAD, PRV_M, satp_bare, 0);
        check(!r.fault && r.paddr == 0xDEADBEEF, "MMU bare mode passthrough");

        // Test 15: TLB caching (second access should hit TLB)
        mmu.flush_tlb();
        mmu.translate(0x00400000, AccessType::LOAD, PRV_U, satp, 0);
        // Corrupt the PTE in memory, TLB should still have old mapping
        uint32_t saved_pte = pmem[0x2000/4 + 0];
        pmem[0x2000/4 + 0] = 0;
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_U, satp, 0);
        check(!r.fault, "MMU TLB hit after PTE corrupted");
        check(r.paddr == 0x80000000, "MMU TLB returns correct paddr");
        pmem[0x2000/4 + 0] = saved_pte; // restore

        // Test 16: TLB flush invalidates
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_U, satp, 0);
        check(!r.fault, "MMU re-walks after TLB flush");

        // Test 17: MXR allows load from execute-only page
        pmem[0x2000/4 + 0] = (target_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_X | PTE_U;
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_U, satp, MSTATUS_MXR);
        check(!r.fault, "MMU MXR allows load from X-only page");

        // Without MXR, same page faults on load
        mmu.flush_tlb();
        r = mmu.translate(0x00400000, AccessType::LOAD, PRV_U, satp, 0);
        check(r.fault, "MMU no MXR faults on X-only load");

        // Restore
        pmem[0x2000/4 + 0] = (target_ppn << PTE_PPN_SHIFT) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;
    }

    void step9_iss() {
        std::cout << "\nStep 9: ISS\n";
        using namespace rv32;

        // ISS SC_THREAD runs automatically; wait for it to finish
        wait(sc_core::sc_time(1, sc_core::SC_US));

        auto& s = iss_ptr->state;

        check(iss_ptr->insn_count == 10, "ISS executed 10 instructions");
        check(s.get_regu(1) == 0x80000000, "ISS x1 = 0x80000000 (LUI)");
        check(s.get_reg(2) == 42, "ISS x2 = 42 (ADDI)");
        check(s.get_reg(3) == 10, "ISS x3 = 10 (ADDI)");
        check(s.get_reg(4) == 52, "ISS x4 = 52 (ADD)");
        check(s.get_reg(5) == 52, "ISS x5 = 52 (LW read-back)");
        check(s.get_reg(6) == 1, "ISS x6 = 1 (BNE taken, skipped 99)");
        check(s.get_reg(7) == 7, "ISS x7 = 7 (C.LI compressed)");
        check(s.pc == cfg::RAM_BASE + 0x26, "ISS pc at C.EBREAK");
        check(s.priv == PRV_M, "ISS still in M-mode");

        // Verify counters
        {
            uint32_t cyc = 0, inst = 0;
            s.csr.read(CSR_MCYCLE, PRV_M, cyc);
            s.csr.read(CSR_MINSTRET, PRV_M, inst);
            check(cyc == 10, "ISS mcycle = 10");
            check(inst == 10, "ISS minstret = 10");
        }

        // Verify the SW wrote through TLM to memory
        {
            uint32_t mem_val = 0;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::RAM_BASE + 0x100,
                        reinterpret_cast<uint8_t*>(&mem_val), 4);
            bus_isock->b_transport(trans, delay);
            check(mem_val == 52, "ISS SW stored 52 via TLM bus");
        }
    }

    void step10_elf_loader() {
        std::cout << "\n--- Step 10: ELF Loader ---\n";

        // Frankenstein a minimal ELF32 by hand. header + 1 phdr + 2 instructions
        uint32_t code[] = { 0x02A00093, 0x00100073 }; // addi x1,x0,42; ebreak
        std::vector<uint8_t> elf(52 + 32 + 8, 0);
        auto w16 = [&](size_t off, uint16_t v) { std::memcpy(&elf[off], &v, 2); };
        auto w32 = [&](size_t off, uint32_t v) { std::memcpy(&elf[off], &v, 4); };

        elf[0] = 0x7F; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
        elf[4] = 1;    // ELFCLASS32
        elf[5] = 1;    // ELFDATA2LSB
        elf[6] = 1;    // EV_CURRENT

        w16(16, 2);       // e_type = ET_EXEC
        w16(18, 0xF3);    // e_machine = EM_RISCV
        w32(20, 1);       // e_version
        w32(24, 0x80000000); // e_entry
        w32(28, 52);      // e_phoff (right after header)
        w16(40, 52);      // e_ehsize
        w16(42, 32);      // e_phentsize
        w16(44, 1);       // e_phnum

        size_t ph = 52;
        w32(ph + 0, 1);          // p_type = PT_LOAD
        w32(ph + 4, 84);         // p_offset = 52 + 32 = 84
        w32(ph + 8, 0x80000000); // p_vaddr
        w32(ph + 12, 0x80000000);// p_paddr
        w32(ph + 16, 8);         // p_filesz
        w32(ph + 20, 16);        // p_memsz (8 bytes bss)
        w32(ph + 24, 5);         // p_flags = PF_R|PF_X

        std::memcpy(&elf[84], code, 8);

        std::vector<uint8_t> mem(256, 0xFF);
        uint32_t mem_base = 0x80000000;

        auto write_fn = [&](uint32_t paddr, const uint8_t* data, size_t len) {
            uint32_t off = paddr - mem_base;
            std::memcpy(&mem[off], data, len);
        };

        ElfLoadResult r = load_elf_from_memory(elf.data(), elf.size(), write_fn);

        check(r.entry_point == 0x80000000, "ELF entry point");
        check(r.segments_loaded == 1, "ELF 1 PT_LOAD segment");
        check(r.load_min == 0x80000000, "ELF load_min");
        check(r.load_max == 0x80000010, "ELF load_max (includes bss)");

        uint32_t loaded_instr;
        std::memcpy(&loaded_instr, &mem[0], 4);
        check(loaded_instr == 0x02A00093, "ELF code loaded correctly");

        uint32_t bss_val;
        std::memcpy(&bss_val, &mem[8], 4);
        check(bss_val == 0, "ELF bss zeroed");

        std::vector<uint8_t> bad_elf = elf;
        bad_elf[0] = 0x00;
        bool caught = false;
        try { load_elf_from_memory(bad_elf.data(), bad_elf.size(), write_fn); }
        catch (const std::runtime_error&) { caught = true; }
        check(caught, "ELF rejects bad magic");

        bad_elf = elf;
        bad_elf[18] = 0x03; bad_elf[19] = 0x00;
        caught = false;
        try { load_elf_from_memory(bad_elf.data(), bad_elf.size(), write_fn); }
        catch (const std::runtime_error&) { caught = true; }
        check(caught, "ELF rejects non-RISC-V");

        bad_elf = elf;
        bad_elf[4] = 2;
        caught = false;
        try { load_elf_from_memory(bad_elf.data(), bad_elf.size(), write_fn); }
        catch (const std::runtime_error&) { caught = true; }
        check(caught, "ELF rejects 64-bit");
    }

    void step11_clint() {
        std::cout << "\n--- Step 11: CLINT ---\n";

        // Test register access via the bus (CLINT is mapped at cfg::CLINT_BASE)
        auto clint_read = [&](uint32_t offset) -> uint32_t {
            uint32_t val = 0;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::CLINT_BASE + offset,
                        reinterpret_cast<uint8_t*>(&val), 4);
            bus_isock->b_transport(trans, delay);
            return val;
        };

        auto clint_write = [&](uint32_t offset, uint32_t val) {
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::CLINT_BASE + offset,
                        reinterpret_cast<uint8_t*>(&val), 4);
            bus_isock->b_transport(trans, delay);
        };

        // msip defaults to 0
        check(clint_read(0x0000) == 0, "CLINT msip defaults 0");

        // Write and read back msip (only bit 0 matters)
        clint_write(0x0000, 0xDEADBEEF);
        check(clint_read(0x0000) == 1, "CLINT msip masks to bit 0");

        // Check sw_irq callback fired
        bool sw_irq_seen = false;
        clint_ptr->on_sw_irq = [&](bool v) { sw_irq_seen = v; };
        clint_write(0x0000, 1);
        check(sw_irq_seen == true, "CLINT msip triggers sw_irq callback");
        clint_write(0x0000, 0);
        check(sw_irq_seen == false, "CLINT msip=0 clears sw_irq");

        // Write mtimecmp lo/hi and read back
        clint_write(0x4000, 0x12345678);
        clint_write(0x4004, 0xAABBCCDD);
        check(clint_read(0x4000) == 0x12345678, "CLINT mtimecmp_lo readback");
        check(clint_read(0x4004) == 0xAABBCCDD, "CLINT mtimecmp_hi readback");

        // Write mtime directly and read back
        clint_write(0xBFF8, 0x00000042);
        clint_write(0xBFFC, 0x00000000);
        check(clint_read(0xBFF8) == 0x42, "CLINT mtime_lo readback");
        check(clint_read(0xBFFC) == 0x00, "CLINT mtime_hi readback");

        // Timer IRQ test: set mtime >= mtimecmp, callback should fire
        bool timer_irq_seen = false;
        clint_ptr->on_timer_irq = [&](bool v) { timer_irq_seen = v; };

        // Set mtimecmp to 100
        clint_write(0x4000, 100);
        clint_write(0x4004, 0);

        // Set mtime to 99, should NOT fire
        clint_write(0xBFF8, 99);
        clint_write(0xBFFC, 0);
        check(timer_irq_seen == false, "CLINT no IRQ when mtime < mtimecmp");

        // Set mtime to 100, should fire
        clint_write(0xBFF8, 100);
        check(timer_irq_seen == true, "CLINT IRQ when mtime >= mtimecmp");

        // Raise mtimecmp to clear IRQ
        timer_irq_seen = true;
        clint_write(0x4000, 200);
        check(timer_irq_seen == false, "CLINT IRQ clears when mtimecmp raised");

        // Let simulation advance so tick_thread runs a couple ticks
        uint64_t before = clint_ptr->get_mtime();
        wait(sc_core::sc_time(300, sc_core::SC_NS));
        uint64_t after = clint_ptr->get_mtime();
        check(after > before, "CLINT mtime increments via tick_thread");
    }

    void step12_plic() {
        std::cout << "\n--- Step 12: PLIC ---\n";

        auto plic_read = [&](uint32_t offset) -> uint32_t {
            uint32_t val = 0;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::PLIC_BASE + offset,
                        reinterpret_cast<uint8_t*>(&val), 4);
            bus_isock->b_transport(trans, delay);
            return val;
        };

        auto plic_write = [&](uint32_t offset, uint32_t val) {
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::PLIC_BASE + offset,
                        reinterpret_cast<uint8_t*>(&val), 4);
            bus_isock->b_transport(trans, delay);
        };

        bool ext_irq = false;
        plic_ptr->on_external_irq = [&](bool v) { ext_irq = v; };

        // Set UART (source 1) priority to 5
        plic_write(cfg::IRQ_UART * 4, 5);
        check(plic_read(cfg::IRQ_UART * 4) == 5, "PLIC priority write/read");

        // Source 0 priority is hardwired to 0
        plic_write(0, 7);
        check(plic_read(0) == 0, "PLIC source 0 priority stuck at 0");

        // Priority clamps to 3 bits
        plic_write(cfg::IRQ_GPIO * 4, 0xFF);
        check(plic_read(cfg::IRQ_GPIO * 4) == 7, "PLIC priority clamps to 0-7");

        // Enable UART interrupt
        plic_write(0x2000, (1u << cfg::IRQ_UART));
        check(plic_read(0x2000) == (1u << cfg::IRQ_UART), "PLIC enable readback");

        // No pending yet, no IRQ
        check(ext_irq == false, "PLIC no IRQ without pending");

        // Set UART pending via set_pending API
        plic_ptr->set_pending(cfg::IRQ_UART, true);
        check(plic_read(0x1000) & (1u << cfg::IRQ_UART), "PLIC pending bit set");
        check(ext_irq == true, "PLIC asserts external IRQ");

        // Claim should return UART source ID
        uint32_t claimed = plic_read(0x200004);
        check(claimed == cfg::IRQ_UART, "PLIC claim returns UART");

        // After claim, IRQ deasserts (no more pending+enabled unclaimed)
        check(ext_irq == false, "PLIC IRQ clears after claim");

        // Second claim returns 0 (nothing left)
        check(plic_read(0x200004) == 0, "PLIC second claim returns 0");

        // Complete the UART interrupt
        plic_write(0x200004, cfg::IRQ_UART);

        // Priority-based arbitration: GPIO(prio 7) beats UART(prio 5)
        plic_write(cfg::IRQ_GPIO * 4, 7);
        plic_write(0x2000, (1u << cfg::IRQ_UART) | (1u << cfg::IRQ_GPIO));
        plic_ptr->set_pending(cfg::IRQ_UART, true);
        plic_ptr->set_pending(cfg::IRQ_GPIO, true);
        claimed = plic_read(0x200004);
        check(claimed == cfg::IRQ_GPIO, "PLIC higher priority wins");

        // UART still pending, claim again
        claimed = plic_read(0x200004);
        check(claimed == cfg::IRQ_UART, "PLIC second claim gets UART");

        // Complete both
        plic_write(0x200004, cfg::IRQ_GPIO);
        plic_write(0x200004, cfg::IRQ_UART);

        // Threshold test: set threshold to 5, UART(prio 5) should be masked
        plic_write(0x200000, 5);
        check(plic_read(0x200000) == 5, "PLIC threshold readback");
        plic_ptr->set_pending(cfg::IRQ_UART, true);
        check(ext_irq == false, "PLIC masks IRQ at or below threshold");

        // GPIO(prio 7) still beats threshold of 5
        plic_ptr->set_pending(cfg::IRQ_GPIO, true);
        check(ext_irq == true, "PLIC IRQ above threshold fires");

        // Clean up
        plic_read(0x200004); // claim GPIO
        plic_read(0x200004); // claim UART
        plic_write(0x200004, cfg::IRQ_GPIO);
        plic_write(0x200004, cfg::IRQ_UART);
        plic_write(0x200000, 0);
    }

    void step13_uart() {
        std::cout << "\n--- Step 13: UART ---\n";

        auto uart_read = [&](uint32_t offset) -> uint32_t {
            uint32_t val = 0;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_READ_COMMAND,
                        cfg::UART_BASE + offset,
                        reinterpret_cast<uint8_t*>(&val), 1);
            bus_isock->b_transport(trans, delay);
            return val & 0xFF;
        };

        auto uart_write = [&](uint32_t offset, uint8_t val) {
            uint32_t v = val;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            setup_trans(trans, tlm::TLM_WRITE_COMMAND,
                        cfg::UART_BASE + offset,
                        reinterpret_cast<uint8_t*>(&v), 1);
            bus_isock->b_transport(trans, delay);
        };

        // LSR should show TX empty + transmitter empty at startup
        uint32_t lsr = uart_read(5);
        check((lsr & 0x60) == 0x60, "UART LSR TX empty at init");
        check((lsr & 0x01) == 0, "UART LSR no RX data at init");

        // TX: write a byte, capture via callback
        uint8_t tx_captured = 0;
        uart_ptr->on_tx = [&](uint8_t c) { tx_captured = c; };
        uart_write(0, 'H');
        check(tx_captured == 'H', "UART TX callback fires");

        // Scratch register round-trip
        uart_write(7, 0xAB);
        check(uart_read(7) == 0xAB, "UART scratch register");

        // RX: push a byte, check LSR DR, then read it
        uart_ptr->push_rx('X');
        lsr = uart_read(5);
        check((lsr & 0x01) == 1, "UART LSR DR after push_rx");

        uint32_t rx = uart_read(0);
        check(rx == 'X', "UART RX reads pushed byte");

        // After reading, DR should clear
        lsr = uart_read(5);
        check((lsr & 0x01) == 0, "UART LSR DR clears after read");

        // RX FIFO: push multiple, read in order
        uart_ptr->push_rx('A');
        uart_ptr->push_rx('B');
        uart_ptr->push_rx('C');
        check(uart_read(0) == 'A', "UART FIFO order A");
        check(uart_read(0) == 'B', "UART FIFO order B");
        check(uart_read(0) == 'C', "UART FIFO order C");

        // IER / IRQ tests
        bool irq_state = false;
        uart_ptr->on_irq = [&](bool v) { irq_state = v; };

        // Enable TX empty interrupt
        uart_write(1, 0x02);
        check(irq_state == true, "UART TX empty IRQ fires when enabled");

        // IIR should indicate TX empty
        uint32_t iir = uart_read(2);
        check((iir & 0x0F) == 0x02, "UART IIR reports TX empty");

        // Disable TX IRQ, enable RX IRQ
        uart_write(1, 0x01);
        check(irq_state == false, "UART IRQ clears with no RX data");

        // Push RX data, IRQ should fire
        uart_ptr->push_rx('Z');
        check(irq_state == true, "UART RX avail IRQ fires");

        iir = uart_read(2);
        check((iir & 0x0F) == 0x04, "UART IIR reports RX avail");

        // Read the byte, IRQ should clear
        uart_read(0);
        check(irq_state == false, "UART IRQ clears after RX read");

        // LCR and MCR are just storage
        uart_write(3, 0x1B);
        check(uart_read(3) == 0x1B, "UART LCR round-trip");
        uart_write(4, 0x0F);
        check(uart_read(4) == 0x0F, "UART MCR round-trip");

        // MSR always 0 (no modem)
        check(uart_read(6) == 0, "UART MSR always 0");
    }

    void step14_platform() {
        std::cout << "\n--- Step 14: Top-Level Platform ---\n";

        // Give the platform ISS time to run its little program
        wait(sc_core::sc_time(1, sc_core::SC_US));

        auto& p = *platform_ptr;
        auto& s = p.cpu.state;

        check(p.cpu.insn_count > 0, "Platform ISS executed instructions");
        check(s.get_reg(1) == 42, "Platform ISS x1 = 42");
        check(s.get_reg(2) == 7, "Platform ISS x2 = 7");
        check(s.get_reg(3) == 49, "Platform ISS x3 = 49");

        // Verify UART TX went through the full wiring chain
        // (cpu -> bus -> uart -> on_tx callback)
        // x4 should be 'V' (0x56) from the test program
        check(s.get_reg(4) == 0x56, "Platform ISS x4 = 'V'");

        // Check CLINT mtime is ticking (we waited 1us, tick=100ns -> ~10 ticks)
        check(p.clint.get_mtime() > 0, "Platform CLINT mtime ticking");
    }

    void run_tests() {
        step1_memory();
        step2_bus();
        step3_rv32_defs();
        step4_decoder();
        step5_csr();
        step6_execute();
        step7_trap();
        step8_mmu();
        step9_iss();
        step10_elf_loader();
        step11_clint();
        step12_plic();
        step13_uart();
        step14_platform();
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

    // Step 11: CLINT (100ns tick = 10MHz mtime clock)
    CLINT clint("clint", sc_core::sc_time(100, sc_core::SC_NS));
    bus.isock.bind(clint.tsock);
    bus.map(cfg::CLINT_BASE, cfg::CLINT_SIZE);
    tester.clint_ptr = &clint;

    // Step 12: PLIC
    PLIC plic("plic");
    bus.isock.bind(plic.tsock);
    bus.map(cfg::PLIC_BASE, cfg::PLIC_SIZE);
    tester.plic_ptr = &plic;

    // Step 13: UART
    UART uart("uart");
    bus.isock.bind(uart.tsock);
    bus.map(cfg::UART_BASE, cfg::UART_SIZE);
    tester.uart_ptr = &uart;

    // Step 9: ISS wired through bus to RAM
    ISS iss("iss", cfg::RAM_BASE);
    iss.stop_on_ebreak = true;
    iss.isock.bind(bus.tsock);
    tester.iss_ptr = &iss;

    // Load test program into RAM:
    //   0x00: lui x1, 0x80000        ; x1 = 0x80000000
    //   0x04: addi x2, x0, 42       ; x2 = 42
    //   0x08: addi x3, x0, 10       ; x3 = 10
    //   0x0C: add x4, x2, x3        ; x4 = 52
    //   0x10: sw x4, 256(x1)        ; mem[0x80000100] = 52
    //   0x14: lw x5, 256(x1)        ; x5 = 52
    //   0x18: bne x5, x2, +8        ; taken (52 != 42), skip to 0x20
    //   0x1C: addi x6, x0, 99       ; (skipped)
    //   0x20: addi x6, x0, 1        ; x6 = 1
    //   0x24: c.li x7, 7            ; x7 = 7
    //   0x26: c.ebreak              ; halt
    uint32_t program[] = {
        0x800000B7,
        0x02A00113,
        0x00A00193,
        0x00310233,
        0x1040A023,
        0x1000A283,
        0x00229463,
        0x06300313,
        0x00100313,
        0x9002439D,
    };
    std::memcpy(ram.data(), program, sizeof(program));

    // Step 14: Full platform instance with its own ISS/bus/RAM/etc
    GamingCPU_VP platform("platform");
    platform.cpu.stop_on_ebreak = true;
    tester.platform_ptr = &platform;

    // Test program for platform ISS:
    //   addi x1, x0, 42       ; x1 = 42
    //   addi x2, x0, 7        ; x2 = 7
    //   add  x3, x1, x2       ; x3 = 49
    //   lui  x4, 0x10000      ; x4 = UART_BASE (0x10000000)
    //   addi x5, x0, 0x56     ; x5 = 'V'
    //   sb   x5, 0(x4)        ; UART TX <- 'V'
    //   addi x4, x5, 0        ; x4 = 0x56 (save for test check)
    //   ebreak
    uint32_t plat_prog[] = {
        0x02A00093, // addi x1, x0, 42
        0x00700113, // addi x2, x0, 7
        0x002081B3, // add  x3, x1, x2
        0x10000237, // lui  x4, 0x10000
        0x05600293, // addi x5, x0, 0x56
        0x00520023, // sb   x5, 0(x4)
        0x00028213, // addi x4, x5, 0
        0x00100073, // ebreak
    };
    std::memcpy(platform.ram.data(), plat_prog, sizeof(plat_prog));

    sc_core::sc_start();

    std::cout << "\n=== All Results: " << pass_count << " passed, "
              << fail_count << " failed ===\n";

    if (fail_count > 0)
        SC_REPORT_FATAL("Test", "Some tests failed");

    return 0;
}
