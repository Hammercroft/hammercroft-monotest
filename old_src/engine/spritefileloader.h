#ifndef SPRITEASSETLOADER_H
#define SPRITEASSETLOADER_H

#include "sprite.h"
#include "spritearena.h"

// Loads a PBM file (P4, raw bits) into a new Sprite allocated from the provided
// arena. Returns nullptr on failure (file not found, format error, out of
// memory).
Sprite *LoadSpritePBM(SpriteArena *arena, const char *filename);

#endif // SPRITEASSETLOADER_H
