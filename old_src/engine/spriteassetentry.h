#ifndef SPRITEASSETENTRY_H
#define SPRITEASSETENTRY_H

#include "sprite.h"
#include <cstdint>

typedef struct {
  uint32_t name_hash; // Hash of sprite name eg. "player_idle"
  Sprite *sprite_ptr; // Pointer into the Arena
} SpriteAssetEntry;

// --- Function Declarations ---
uint32_t HashSpriteName(const char *str); // Renamed to avoid collision or
                                          // static? Or just reuse name?
// To avoid collision with `HashString` in bkg manager if they are linked
// together without namespaces, we should probably make HashString static in the
// cpp files or specialized. For now, let's declare them.

void RegisterSpriteAsAsset(SpriteAssetEntry *table, size_t table_size,
                           const char *name, Sprite *sprite);
Sprite *GetSprite(SpriteAssetEntry *table, size_t table_size, const char *name);

#endif