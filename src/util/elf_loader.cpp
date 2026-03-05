#include "elf_loader.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace {

// We define our own ELF structs so we don't need host <elf.h>. Portable baby
constexpr uint8_t EI_NIDENT = 16;

struct Elf32_Ehdr {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

constexpr uint32_t PT_LOAD = 1;
constexpr uint16_t EM_RISCV = 0xF3;
constexpr uint16_t ET_EXEC = 2;

void validate_header(const Elf32_Ehdr& eh) {
    // 7F 45 4C 46 or go home
    if (eh.e_ident[0] != 0x7F || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L'  || eh.e_ident[3] != 'F')
        throw std::runtime_error("ELF: bad magic");

    if (eh.e_ident[4] != 1)
        throw std::runtime_error("ELF: not 32-bit");

    if (eh.e_ident[5] != 1)
        throw std::runtime_error("ELF: not little-endian");

    if (eh.e_machine != EM_RISCV)
        throw std::runtime_error("ELF: not RISC-V (machine=" +
                                 std::to_string(eh.e_machine) + ")");

    if (eh.e_type != ET_EXEC)
        throw std::runtime_error("ELF: not an executable");
}

ElfLoadResult do_load(const uint8_t* data, size_t size, elf_write_fn write) {
    if (size < sizeof(Elf32_Ehdr))
        throw std::runtime_error("ELF: file too small");

    Elf32_Ehdr eh;
    std::memcpy(&eh, data, sizeof(eh));
    validate_header(eh);

    ElfLoadResult result;
    result.entry_point = eh.e_entry;

    size_t ph_end = (size_t)eh.e_phoff + (size_t)eh.e_phnum * eh.e_phentsize;
    if (ph_end > size)
        throw std::runtime_error("ELF: program headers exceed file");

    for (uint16_t i = 0; i < eh.e_phnum; i++) {
        Elf32_Phdr ph;
        std::memcpy(&ph, data + eh.e_phoff + i * eh.e_phentsize, sizeof(ph));

        if (ph.p_type != PT_LOAD)
            continue;

        if (ph.p_offset + ph.p_filesz > size)
            throw std::runtime_error("ELF: segment data exceeds file");

        if (ph.p_filesz > 0)
            write(ph.p_paddr, data + ph.p_offset, ph.p_filesz);

        // memsz > filesz means bss. gotta zero it or globals start as garbage
        if (ph.p_memsz > ph.p_filesz) {
            size_t bss_len = ph.p_memsz - ph.p_filesz;
            std::vector<uint8_t> zeros(bss_len, 0);
            write(ph.p_paddr + ph.p_filesz, zeros.data(), bss_len);
        }

        if (ph.p_paddr < result.load_min)
            result.load_min = ph.p_paddr;
        if (ph.p_paddr + ph.p_memsz > result.load_max)
            result.load_max = ph.p_paddr + ph.p_memsz;

        result.segments_loaded++;
    }

    return result;
}

} // anonymous namespace

ElfLoadResult load_elf_from_memory(const uint8_t* data, size_t size, elf_write_fn write) {
    return do_load(data, size, write);
}

ElfLoadResult load_elf(const std::string& path, elf_write_fn write) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("ELF: cannot open " + path);

    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), size);

    return do_load(buf.data(), buf.size(), write);
}
