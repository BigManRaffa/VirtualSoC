#ifndef GAMINGCPU_VP_SD_CARD_MODEL_H
#define GAMINGCPU_VP_SD_CARD_MODEL_H

#include <cstdint>
#include <string>
#include <fstream>
#include <vector>

// Virtual SD card backed by a host-side .img file
class SDCardModel
{
public:
    bool open(const std::string& path);
    bool is_open() const { return file_.is_open(); }
    bool read_block(uint32_t block_addr, uint8_t* buf);

private:
    std::ifstream file_;
    static constexpr size_t BLOCK_SIZE = 512;
};

#endif // GAMINGCPU_VP_SD_CARD_MODEL_H
