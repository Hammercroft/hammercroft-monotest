#ifndef SPRITE_H
#define SPRITE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct __attribute__((aligned(4))) Sprite {
  // 1. Metadata (Compact)
  int16_t width;  // e.g., 32, 64, 96...
  int16_t height; // e.g., 32, 64...

  // 2. The "Stride"
  // Pre-calculate this! (width / 32).
  // Used for your word-loops so you don't divide at runtime.
  int32_t width_in_words;

  // 3. The Data
  // We use a "Flexible Array Member" (C99 feature).
  // The data sits *inside* the struct allocation, not a pointer to elsewhere.
  // This reduces cache misses (1 fetch for struct + data).
  uint32_t pixels[];
} Sprite;

#endif