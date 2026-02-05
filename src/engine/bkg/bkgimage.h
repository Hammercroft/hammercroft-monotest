#ifndef BKGIMAGE_H
#define BKGIMAGE_H

#include <stddef.h>
#include <stdint.h>

namespace mtengine {

// Forward declaration
struct BkgImage;

// A canvas-sized image, ideally memcpy'd into the the canvas buffer before
// drawing sprites in a frame.

typedef struct __attribute__((aligned(16))) BkgImage {
  // Dimensions
  int32_t width;
  int32_t height;

  // Stride Optimization
  // (width / 32). Useful if we ever need to scroll it partially,
  // though usually we just copy the whole block.
  int32_t width_in_words;

  // Padding
  // 4+4+4 = 12 bytes. We add 4 bytes of padding to ensure 'pixels'
  // starts at offset 16 (128-bit aligned) for fastest SIMD loading.
  int32_t _padding;

  // The Raw Data
  // Flexible Array Member: The data sits immediately after the struct.
  // If you are worrying about struct alignment, this field does not count
  // -- this takes up 0 bytes in this struct.
  uint32_t pixels[];
} BkgImage;

} // namespace mtengine

#endif // BKGIMAGE_H