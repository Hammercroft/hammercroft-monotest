#include "engine.h"
#include "igame.h"
#include <chrono>
#include <cstring>
#include <thread>

// COMMON-SIDE ENGINE IMPLEMENTATION, SOURCE

namespace mtengine {

const int ENGINE_FRAME_INTERVAL = 20; // 20ms = 50fps

void Engine::play(IGame &game) {
  std::cout << "Engine::play() called\n";

  // Prepare for main game loop
  using clock = std::chrono::steady_clock;
  auto last_frame_start = clock::now();
  const float frame_interval_seconds = ENGINE_FRAME_INTERVAL / 1000.0f;
  const std::chrono::duration<float> min_frame_time(frame_interval_seconds);
  const float fixed_dt = frame_interval_seconds; // for ECS
  float accumulator = 0.0f;

  // Main game loop
  while (Engine::main_game_loop_running) {
    Engine::tick++;
    auto current_frame_start = clock::now();

    // Calculate actual time passed since last loop iteration
    std::chrono::duration<float> elapsed =
        current_frame_start - last_frame_start;
    float dt = elapsed.count();
    last_frame_start = current_frame_start;

    // Prevent "Spiral of Death": cap dt if the window was moved or app frozen
    if (dt > 0.25f)
      dt = 0.25f;

    accumulator += dt;

    // Poll platform events
    poll_events();
    game.early_update(*this, dt);
    while (accumulator >= fixed_dt) {
      // <- CALL ENGINE ECS PROCESSING HERE, pass fixed_dt
      accumulator -= fixed_dt;
    }
    game.update(*this, dt);
    draw_prepare();
    draw_lists();
    draw_present();

    // Hybrid Spin-Lock
    auto target_wake_time = current_frame_start + min_frame_time;
    // Phase 1: Polite Sleep: Give back CPU if we have more than 1.5ms to wait
    // We use 1.5ms as a buffer because OS sleep is often imprecise
    auto time_left = target_wake_time - clock::now();
    if (time_left > std::chrono::microseconds(1500)) {
      std::this_thread::sleep_for(time_left - std::chrono::microseconds(1000));
    }
    // Phase 2: Busy-Wait: Spin the CPU for the final precision gap
    while (clock::now() < target_wake_time) {
      // Prevent -03 from throwing away this loop
      __asm__ __volatile__("" ::: "memory");
    }
  }
}

void Engine::clear_scene() {
  std::cout << "clear_scene() called...\n";
  bkg_manager.clear();
}

void Engine::unload_all() {
  std::cout << "unload_all() called...\n";
  bkg_manager.clear();
}

BkgImage *Engine::load_bkg_image(const char *filename) {
  /*std::cout << "load_bkg_image() called for " << filename << "\n";
  BkgImage *img = LoadBkgImagePBM(&bkg_arena, filename);
  if (img) {
    // Store in lookup table
    RegisterBkgImageAsAsset(bkg_lookup_table, BKG_TABLE_SIZE, filename, img);
  }
  return img;*/
  // return nullptr; // TODO implement
  return bkg_manager.load(filename);
}

void Engine::set_active_background(BkgImage *bkg) { active_background = bkg; }

bool Engine::try_set_active_background(BkgImage *bkg) {
  if (bkg) {
    active_background = bkg;
    return true;
  }
  std::cerr << "Warning: try_set_active_background called with null image.\n";
  return false;
}

BkgImage *Engine::get_active_background() { return active_background; }

void Engine::set_invert_colors(bool enabled) { invert_colors = enabled; }
bool Engine::get_invert_colors() { return invert_colors; }

void Engine::set_dead_space_is_white(bool is_white) {
  dead_space_is_white = is_white;
}
bool Engine::get_dead_space_is_white() { return dead_space_is_white; }

void Engine::init_asset_management(size_t sprite_mem_size,
                                   size_t bkg_mem_size) {
  // bkg_arena.base_memory = (uint8_t *)malloc(bkg_mem_size);
  // bkg_arena.capacity = bkg_mem_size;
  // bkg_arena.bytes_used = 0;
  // memset(bkg_lookup_table, 0, sizeof(bkg_lookup_table));
  bkg_manager = BkgImageManager(bkg_mem_size);
}

Engine *singleton() {
  static Engine instance;
  return &instance;
}

} // namespace mtengine