#ifndef GAMINGCPU_VP_PALETTE_H
#define GAMINGCPU_VP_PALETTE_H

#include <cstdint>

// PLAYPAL: 256 entries x 3 bytes (RGB) x 14 palettes
// COLORMAP: 256 entries x 34 maps (brightness levels)

namespace palette {

struct RGB { uint8_t r, g, b; };

void load_playpal(const uint8_t* mem, int palette_index, RGB out[256]);

uint8_t apply_colormap(const uint8_t* colormap_data, int map_index, uint8_t pixel);

} // namespace palette

#endif // GAMINGCPU_VP_PALETTE_H
