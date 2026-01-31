#ifndef BKGIMAGEASSETENTRY_H
#define BKGIMAGEASSETENTRY_H

#include "bkgimage.h"
#include <cstdint>

typedef struct {
  uint32_t name_hash; // Hash of bkg name eg. "testbackground"
  BkgImage *bkg_ptr;  // Pointer into the Arena
} BkgImageAssetEntry;

// --- Function Declarations ---
uint32_t HashString(const char *str);
void RegisterBkgImageAsAsset(BkgImageAssetEntry *table, size_t table_size,
                             const char *name, BkgImage *bkg);
BkgImage *GetBkgImage(BkgImageAssetEntry *table, size_t table_size,
                      const char *name);

#endif