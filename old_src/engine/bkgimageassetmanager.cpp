#include "bkgimageassetentry.h"
#include <cstring>
#include <iostream>

// --- Helper: DJB2 Hash ---
uint32_t HashString(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

void RegisterBkgImageAsAsset(BkgImageAssetEntry *table, size_t table_size,
                             const char *name, BkgImage *bkg) {
  if (!bkg)
    return;

  uint32_t hash = HashString(name);
  size_t index = hash % table_size;
  size_t start_index = index;

  while (true) {
    // 1. Found an empty slot?
    if (table[index].bkg_ptr == NULL) {
      table[index].name_hash = hash;
      table[index].bkg_ptr = bkg;
      // std::cout << "Registered asset '" << name << "' at index " << index <<
      // std::endl;
      return;
    }

    // 2. Found existing slot with same hash? (Overwrite/Update)
    // Note: This collision resolution is simple; we assume different strings
    // won't collide often in a small table, or we intentionally overwrite.
    if (table[index].name_hash == hash) {
      table[index].bkg_ptr = bkg;
      return;
    }

    // 3. Keep probing
    index = (index + 1) % table_size;

    // 4. Wrapped around to start? Table full.
    if (index == start_index) {
      std::cerr << "Error: BkgAssetTable full! Cannot register " << name
                << std::endl;
      return;
    }
  }
}

BkgImage *GetBkgImage(BkgImageAssetEntry *table, size_t table_size,
                      const char *name) {
  uint32_t hash = HashString(name);
  size_t index = hash % table_size;
  size_t start_index = index;

  while (true) {
    // 1. Found empty slot? It's not here.
    if (table[index].bkg_ptr == NULL) {
      return NULL;
    }

    // 2. Found match?
    if (table[index].name_hash == hash) {
      return table[index].bkg_ptr;
    }

    // 3. Keep probing
    index = (index + 1) % table_size;

    // 4. Wrapped around? Not found.
    if (index == start_index) {
      return NULL;
    }
  }
}
