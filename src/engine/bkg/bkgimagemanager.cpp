#include "bkgimagemanager.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace mtengine {

// --- Helper Functions ---
static uint32_t HashString(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

static void skip_comments(std::ifstream &fs) {
  char ch;
  while (fs.get(ch)) {
    if (std::isspace(ch)) {
      continue;
    }
    if (ch == '#') {
      fs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }
    fs.unget();
    break;
  }
}

// --- BkgImageManager Implementation ---

BkgImageManager::BkgImageManager(size_t mem_size)
    : generation(0), bytes_used(0) {
  // Round down to multiple of 16
  capacity = (mem_size / 16) * 16;
  base_memory = (uint8_t *)malloc(capacity);
  if (!base_memory) {
    std::cerr << "Failed to allocate BkgImageManager memory of size "
              << capacity << std::endl;
    capacity = 0;
  }
}

BkgImageManager::~BkgImageManager() {
  if (base_memory) {
    free(base_memory);
    base_memory = nullptr;
  }
}

// Move Constructor
BkgImageManager::BkgImageManager(BkgImageManager &&other) noexcept
    : generation(other.generation), capacity(other.capacity),
      bytes_used(other.bytes_used),
      lookup_table(std::move(other.lookup_table)) {
  base_memory = other.base_memory;
  other.base_memory = nullptr;
  other.capacity = 0;
  other.bytes_used = 0;
}

// Move Assignment
BkgImageManager &BkgImageManager::operator=(BkgImageManager &&other) noexcept {
  if (this != &other) {
    // Free existing
    if (base_memory) {
      free(base_memory);
    }

    // Take ownership
    base_memory = other.base_memory;
    capacity = other.capacity;
    bytes_used = other.bytes_used;
    generation = other.generation;
    lookup_table = std::move(other.lookup_table);

    // Reset source
    other.base_memory = nullptr;
    other.capacity = 0;
    other.bytes_used = 0;
  }
  return *this;
}

void BkgImageManager::clear() {
  bytes_used = 0;
  lookup_table.clear();
  generation++;
}

BkgImage *BkgImageManager::find(const char *filename) {
  uint32_t hash = HashString(filename);
  for (const auto &entry : lookup_table) {
    if (entry.hash == hash) {
      return entry.image;
    }
  }
  return nullptr;
}

BkgImage *BkgImageManager::load(const char *filename) {
  // 1. Check if already loaded
  BkgImage *existing = find(filename);
  if (existing)
    return existing;

  // 2. Open File
  std::ifstream fs(filename, std::ios::binary);
  if (!fs) {
    std::cerr << "Error: Could not open file " << filename << std::endl;
    return nullptr;
  }

  // 3. Parse Header (PBM P4)
  std::string line;
  if (!std::getline(fs, line) || line.length() < 2 || line[0] != 'P' ||
      line[1] != '4') {
    std::cerr << "Error: Invalid PBM format in " << filename << " (Expected P4)"
              << std::endl;
    return nullptr;
  }

  int width, height;
  skip_comments(fs);
  if (!(fs >> width))
    return nullptr;
  skip_comments(fs);
  if (!(fs >> height))
    return nullptr;

  if (width % 32 != 0) {
    std::cerr << "Error: BkgImage " << filename << " width (" << width
              << ") is not a multiple of 32." << std::endl;
    return nullptr;
  }

  // Consume whitespace
  char ch;
  if (fs.get(ch) && !std::isspace(ch)) {
    fs.unget();
  }

  // 4. Calculate Size
  int32_t words_per_row = width / 32;
  int32_t bytes_per_row = words_per_row * 4;
  size_t total_data_bytes = bytes_per_row * height;
  size_t struct_size = sizeof(BkgImage) + total_data_bytes;

  // 5. Allocate in Arena
  // Alignment: We need 16-byte alignment.
  // base_memory is malloc'd (usually 16-byte aligned on 64-bit systems).
  // Our current_ptr logic needs to verify.

  uintptr_t current_ptr = (uintptr_t)(base_memory + bytes_used);
  size_t align = 16;
  size_t offset = 0;
  if (current_ptr % align != 0) {
    offset = align - (current_ptr % align);
  }

  if (bytes_used + offset + struct_size > capacity) {
    std::cerr << "Error: BkgImageManager out of memory!" << std::endl;
    return nullptr;
  }

  bytes_used += offset;
  BkgImage *img = (BkgImage *)(base_memory + bytes_used);
  bytes_used += struct_size;

  // 6. Init Struct
  img->width = width;
  img->height = height;
  img->width_in_words = words_per_row;
  img->_padding = 0;

  // 7. Read Pixels
  fs.read((char *)img->pixels, total_data_bytes);

  if (!fs && fs.gcount() != (std::streamsize)total_data_bytes) {
    std::cerr << "Warning: File " << filename << " ended early." << std::endl;
  }

  // 8. Add to Lookup
  Entry entry;
  entry.hash = HashString(filename);
  entry.image = img;
  lookup_table.push_back(entry);

  return img;
}

} // namespace mtengine