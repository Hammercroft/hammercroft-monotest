#ifndef BKGIMAGEMANAGER_H
#define BKGIMAGEMANAGER_H

#include "bkgimage.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace mtengine {

class BkgImageManager {
public:
  explicit BkgImageManager(
      size_t mem_size); // NOTE, do not use directly use given size,
                        // round to the nearest multiple of 16 bytes
                        // (the size of a BkgImage) that is lesser
                        // or equal than given

  // Rule of 5: Disable copying, enable moving
  BkgImageManager(const BkgImageManager &) = delete;
  BkgImageManager &operator=(const BkgImageManager &) = delete;
  BkgImageManager(BkgImageManager &&other) noexcept;
  BkgImageManager &operator=(BkgImageManager &&other) noexcept;
  int generation;
  ~BkgImageManager();
  void clear(); // clears both memory and lookup table to a blank slate
                // this should also increment the generation number
                // for easy invalidation of ECS Drawable components
  BkgImage *load(const char *filename); // load into memory, and add to lookup
  BkgImage *find(const char *filename); // find in lookup table

private:
  uint8_t *base_memory = nullptr;
  size_t capacity = 0;
  size_t bytes_used = 0;

  struct Entry {
    uint32_t hash;
    BkgImage *image;
  };

  std::vector<Entry> lookup_table;
};

} // namespace mtengine

#endif // BKGIMAGEMANAGER_H