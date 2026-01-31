#ifndef BKGIMAGEARENA_H
#define BKGIMAGEARENA_H

#include <cstddef>
#include <cstdint>

// #define BKG_ARENA_SIZE (16 * 1024 * 1024) // 16 MB

typedef struct {
  uint8_t *base_memory; // Pointer to the start of the block
  size_t bytes_used;    // How much we have filled so far
  size_t capacity;      // Total size
} BkgImageArena;

#endif
