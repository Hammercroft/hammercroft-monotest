#ifndef SPRITEARENA_H
#define SPRITEARENA_H

#include <cstddef>
#include <cstdint>

// #define SPRITE_ARENA_SIZE (32 * 1024 * 1024) // 32 MB

typedef struct {
  uint8_t *base_memory; // Pointer to the start of the 32MB block
  size_t bytes_used;    // How much we have filled so far
  size_t capacity;      // Total size (32MB)
} SpriteArena;

/*
// 1. The Storage
uint8_t *GlobalArena;
size_t ArenaHead = 0;

// 2. The Allocation Function (Replaces malloc)
void* ArenaAlloc(size_t size) {
    // Alignment (Mechanical Sympathy!)
    // Round up 'size' to the next multiple of 32 bytes to keep everything
aligned size = (size + 31) & ~31;

    if (ArenaHead + size > ARENA_SIZE) return NULL; // Out of Memory!

    void *ptr = GlobalArena + ArenaHead;
    ArenaHead += size;
    return ptr;
}

// 3. The "Checkpoint" (For unloading)
size_t GetArenaCheckpoint() {
    return ArenaHead;
}

void RewindArena(size_t checkpoint) {
    ArenaHead = checkpoint;
    // Everything allocated after 'checkpoint' is now effectively freed.
}
*/
#endif