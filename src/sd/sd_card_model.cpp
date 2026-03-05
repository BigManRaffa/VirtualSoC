#include "sd_card_model.h"
#include <cstring>

bool SDCardModel::open(const std::string& path) {
    file_.open(path, std::ios::binary);
    return file_.is_open();
}

bool SDCardModel::read_block(uint32_t block_addr, uint8_t* buf) {
    if (!file_.is_open())
        return false;

    file_.seekg(static_cast<std::streamoff>(block_addr) * BLOCK_SIZE);
    if (!file_)
        return false;

    file_.read(reinterpret_cast<char*>(buf), BLOCK_SIZE);
    return file_.gcount() == static_cast<std::streamsize>(BLOCK_SIZE);
}
