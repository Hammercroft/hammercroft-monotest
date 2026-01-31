#include "spriteassetentry.h"
#include <cstring>
#include <iostream>

// --- Helper: DJB2 Hash ---
// Static to avoid linker collision with Bkg's HashString
static uint32_t HashString(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

void RegisterSpriteAsAsset(SpriteAssetEntry *table, size_t table_size,
                           const char *name, Sprite *sprite) {
  if (!sprite)
    return;

  uint32_t hash = HashString(name);
  size_t index = hash % table_size;
  size_t start_index = index;

  while (true) {
    // 1. Found an empty slot?
    if (table[index].sprite_ptr == NULL) {
      table[index].name_hash = hash;
      table[index].sprite_ptr = sprite;
      // std::cout << "Registered sprite '" << name << "' at index " << index <<
      // std::endl;
      return;
    }

    // 2. Found existing slot with same hash? (Overwrite/Update)
    if (table[index].name_hash == hash) {
      table[index].sprite_ptr = sprite;
      return;
    }

    // 3. Keep probing
    index = (index + 1) % table_size;

    // 4. Wrapped around to start? Table full.
    if (index == start_index) {
      std::cerr << "Error: SpriteAssetTable full! Cannot register " << name
                << std::endl;
      return;
    }
  }
}

Sprite *GetSprite(SpriteAssetEntry *table, size_t table_size,
                  const char *name) {
  uint32_t hash = HashString(name);
  size_t index = hash % table_size;
  size_t start_index = index;

  while (true) {
    // 1. Found empty slot? It's not here.
    if (table[index].sprite_ptr == NULL) {
      return NULL;
    }

    // 2. Found match?
    if (table[index].name_hash == hash) {
      return table[index].sprite_ptr;
    }

    // 3. Keep probing
    index = (index + 1) % table_size;

    // 4. Wrapped around? Not found.
    if (index == start_index) {
      return NULL;
    }
  }
}
