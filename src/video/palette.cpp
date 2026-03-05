#include "palette.h"
#include <cstring>

namespace palette {

void load_playpal(const uint8_t* mem, int palette_index, RGB out[256]) {
    const uint8_t* base = mem + palette_index * 768;
    for (int i = 0; i < 256; i++) {
        out[i].r = base[i * 3 + 0];
        out[i].g = base[i * 3 + 1];
        out[i].b = base[i * 3 + 2];
    }
}

uint8_t apply_colormap(const uint8_t* colormap_data, int map_index, uint8_t pixel) {
    return colormap_data[map_index * 256 + pixel];
}

} // namespace palette
