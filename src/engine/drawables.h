#ifndef DRAWABLES_H
#define DRAWABLES_H

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
#define _Static_assert static_assert
#endif

// Forward declaration
typedef struct Sprite Sprite;

// --- Shared Flags ---
#define DRAW_FLAG_HIDDEN (1 << 0)
#define DRAW_FLAG_INVERT                                                       \
  (1 << 1) // drawing sprite will result in inverting colors instead of painting

// --- The Common Layout ---
// 32 Bytes, 16-byte aligned.
#define DRAWABLE_BODY                                                          \
  Sprite *sprite;    /* 8 bytes (Offset 0) */                                  \
  Sprite *mask;      /* 8 bytes (Offset 8) */                                  \
  uint32_t sort_key; /* 4 bytes (Offset 16) */                                 \
  uint32_t flags;    /* 4 bytes (Offset 20) */                                 \
  uint32_t owner_id; /* 4 bytes (Offset 24) */                                 \
  int16_t x;         /* 2 bytes (Offset 28) */                                 \
  int16_t y;         /* 2 bytes (Offset 30) */
// NOTE THAT THE MASK IS NOT OPTIONAL!

// --- Layer 1: Background Objects (Parallax, Clouds, Distant Mountains) ---
// Rendered after the base canvas fill, but before the isometric world.
// sort_key = z-index
typedef struct __attribute__((aligned(16))) BackgroundDrawable {
  DRAWABLE_BODY
} BackgroundDrawable;

// --- Layer 2: The Isometric World (Player, Walls, Trees) ---
// Rendered after background objects and before foreground objects.
// sort_key = (Feet_Y << 16) | X
typedef struct __attribute__((aligned(16))) WorldDrawable {
  DRAWABLE_BODY
} WorldDrawable;

// --- Layer 3: Foreground / UI (HUD, Text, Cutscenes) ---
// Rendered last on top of everything.
// sort_key = z-index
typedef struct __attribute__((aligned(16))) ForegroundDrawable {
  DRAWABLE_BODY
} ForegroundDrawable;

// --- Generic Union ---
// For the low-level blitter that doesn't care about layers.
typedef union {
  BackgroundDrawable bg;
  WorldDrawable world;
  ForegroundDrawable fg;
} GenericDrawable;

// --- Static Assertions ---
_Static_assert(sizeof(BackgroundDrawable) == 32,
               "BackgroundDrawable size mismatch");
_Static_assert(sizeof(WorldDrawable) == 32, "WorldDrawable size mismatch");
_Static_assert(sizeof(ForegroundDrawable) == 32,
               "ForegroundDrawable size mismatch");

#endif // DRAWABLES_H