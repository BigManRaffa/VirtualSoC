#ifndef GAMINGCPU_VP_ELF_LOADER_H
#define GAMINGCPU_VP_ELF_LOADER_H

#include <cstdint>
#include <string>
#include <functional>

struct ElfLoadResult {
    uint32_t entry_point = 0;
    uint32_t load_min = 0xFFFFFFFF;
    uint32_t load_max = 0;
    int segments_loaded = 0;
};

using elf_write_fn = std::function<void(uint32_t, const uint8_t*, size_t)>;

ElfLoadResult load_elf(const std::string& path, elf_write_fn write);
ElfLoadResult load_elf_from_memory(const uint8_t* data, size_t size, elf_write_fn write);

#endif // GAMINGCPU_VP_ELF_LOADER_H
