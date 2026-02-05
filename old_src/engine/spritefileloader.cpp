#include "spritefileloader.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

// --- Helper: Skip Comments & Whitespace (Duplicated from bkgimageloader) ---
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

// --- Helper: Arena Allocator for SpriteArena ---
static void *Arena_Alloc(SpriteArena *arena, size_t size, size_t align) {
  // 1. Calculate current address
  uintptr_t current_ptr = (uintptr_t)(arena->base_memory + arena->bytes_used);

  // 2. Calculate alignment offset
  size_t offset = 0;
  if (align > 0) {
    size_t modulo = current_ptr % align;
    if (modulo != 0) {
      offset = align - modulo;
    }
  }

  // 3. Check capacity
  if (arena->bytes_used + offset + size > arena->capacity) {
    std::cerr << "Error: SpriteArena out of memory!" << std::endl;
    return nullptr;
  }

  // 4. Update usage and return aligned pointer
  arena->bytes_used += offset;
  void *result = arena->base_memory + arena->bytes_used;
  arena->bytes_used += size;

  return result;
}

Sprite *LoadSpritePBM(SpriteArena *arena, const char *filename) {
  std::ifstream fs(filename, std::ios::binary);
  if (!fs) {
    std::cerr << "Error: Could not open file " << filename << std::endl;
    return nullptr;
  }

  // 1. Check Magic Number (Must be "P4")
  std::string line;
  if (!std::getline(fs, line) || line.length() < 2 || line[0] != 'P' ||
      line[1] != '4') {
    std::cerr << "Error: Invalid PBM format in " << filename << " (Expected P4)"
              << std::endl;
    return nullptr;
  }

  // 2. Parse Dimensions
  int width, height;

  skip_comments(fs);
  if (!(fs >> width))
    return nullptr;

  skip_comments(fs);
  if (!(fs >> height))
    return nullptr;

  // 3. Enforce Mechanical Sympathy (The 32-pixel rule)
  // Essential for our 32-bit word processing loop
  if (width % 32 != 0) {
    std::cerr << "Error: Sprite " << filename << " width (" << width
              << ") is not a multiple of 32." << std::endl;
    return nullptr;
  }

  // 4. Consume the single whitespace byte after the header
  char ch;
  if (fs.get(ch) && !std::isspace(ch)) {
    fs.unget();
  }

  // 5. Calculate Allocation Size
  int32_t words_per_row = width / 32;
  int32_t bytes_per_row = words_per_row * 4;
  size_t total_data_bytes = bytes_per_row * height;

  // 6. Allocate from Arena
  // Sprite struct has __attribute__((aligned(4))), but we can align to 4 bytes
  // or more.
  Sprite *s =
      (Sprite *)Arena_Alloc(arena, sizeof(Sprite) + total_data_bytes, 4);

  if (!s) {
    return nullptr;
  }

  // 7. Fill Metadata
  s->width = (int16_t)width;
  s->height = (int16_t)height;
  s->width_in_words = words_per_row;

  // 8. Read the Bits
  fs.read((char *)s->pixels, total_data_bytes);

  if (!fs) {
    if (fs.gcount() != (std::streamsize)total_data_bytes) {
      std::cerr << "Warning: File " << filename << " ended early (Corrupt?)"
                << std::endl;
    }
  }

  return s;
}
