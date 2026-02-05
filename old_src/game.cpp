#include "game.h"
#include "engine/engine.h"
#include <iostream>
#include <string.h>

////////////////////////////////////////////////
/////////// GAME LIFECYCLE FUNCTIONS ///////////
////////////////////////////////////////////////

// Game initialization function
void Game::init(Engine &engine) {
  engine.set_registry(&registry);

  // 1. Initialize Arenas (Allocate the huge raw blocks once) & prepare lookup
  // tables
  sprite_arena.base_memory = (uint8_t *)malloc(SPRITE_ARENA_SIZE);
  sprite_arena.capacity = SPRITE_ARENA_SIZE;
  sprite_arena.bytes_used = 0;

  bkg_arena.base_memory = (uint8_t *)malloc(BKG_ARENA_SIZE);
  bkg_arena.capacity = BKG_ARENA_SIZE;
  bkg_arena.bytes_used = 0;

  memset(sprite_table, 0, sizeof(sprite_table));
  memset(bkg_table, 0, sizeof(bkg_table));

  // 2. Initialize lua scripting engine
  if (scripting.init(this, &engine, &registry)) {
    if (scripting.load_script("./assets/lua/init.lua")) {
      scripting.run_script();
    }
  }

  std::cout << "Init Done.\n";
}

// Runs after all ECS components (except Drawables) are processed
void Game::post_ecs_update(Engine &engine) {}

// Runs before all ECS components (except Drawables) are processed
void Game::pre_ecs_update(Engine &engine) {}