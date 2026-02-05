#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <cstddef>

////////////////////
// GAME CONSTANTS //
////////////////////

// Game canvas size
#define GAME_CANVAS_WIDTH 480
#define GAME_CANVAS_HEIGHT 320
#define GAME_CANVAS_SCALE 2.0f
// GAME_CANVAS_SCALE sets the initial size of the window relative to the
// canvas size. This should be any positive integer represented as a float.

// Image bitmap storage limits
const size_t SPRITE_MEM_SIZE = 512 * 1024; // 512 kibibytes
const size_t BKG_MEM_SIZE = 64 * 1024;     // 64 kibibytes

#endif // GAMEINFO_H