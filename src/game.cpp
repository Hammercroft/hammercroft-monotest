#include "game.h"
#include "engine/bkgimagefileloader.h"
#include "engine/drawables.h"
#include "engine/ecs.h"
#include "engine/engine.h"
#include "engine/spritefileloader.h"
#include <iostream>
#include <string.h>

// Helper to log bounces (simplified port of on_bounce)
static void on_bounce(Engine &engine) {
  std::cout << "Bounce!" << std::endl;
  engine.play_sound("./assets/snd/boing.wav");
}

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

  // 3. Load Assets (TODO do all these in init.lua)
  // Preload Sound (note that we have a relatively dynamic sound loading system)
  engine.load_sound("./assets/snd/boing.wav");

  Sprite *sprite_test =
      LoadSpritePBM(&sprite_arena, "./assets/spr/testball.pbm");

  if (sprite_test) {
    RegisterSpriteAsAsset(sprite_table, SPRITE_TABLE_SIZE, "testball",
                          sprite_test);
  }

  Sprite *sprite_test2 = GetSprite(sprite_table, SPRITE_TABLE_SIZE, "testball");
  if (sprite_test2) {
    std::cout << "Successfully retrieved testball" << std::endl;

    // Create multiple bouncing entities
    for (int i = 0; i < 2; i++) {
      EntityID entity = registry.create_entity();

      ForegroundDrawable fd;
      fd.sprite = sprite_test2;
      fd.mask = sprite_test2;
      fd.sort_key = 0;
      fd.flags = DRAW_FLAG_INVERT;
      fd.owner_id = entity;
      // Initial positions staggered
      fd.x = 50 + (i * 30);
      fd.y = 50 + (i * 10);

      int index = engine.add_foreground_drawable(fd);
      registry.set_drawable_ref(entity, DrawableType::FOREGROUND, index);

      // Add Displaceable Component
      float vx = 12.0f + (i * 0.5f);
      float vy = 6.5f + (i * 0.4f);
      registry.set_displaceable(entity, (float)fd.x, (float)fd.y, vx, vy);
    }
  }

  // Background Loading ...
  BkgImage *bkg_test =
      LoadBkgImagePBM(&bkg_arena, "./assets/bkg/testbackground.pbm");

  if (bkg_test) {
    RegisterBkgImageAsAsset(bkg_table, BKG_TABLE_SIZE, "testbackground",
                            bkg_test);
  }

  BkgImage *bkg_test2 =
      GetBkgImage(bkg_table, BKG_TABLE_SIZE, "testbackground");
  if (bkg_test2) {
    std::cout << "Successfully retrieved testbackground" << std::endl;
    engine.set_active_background(bkg_test2);
  }
}

void Game::update(Engine &engine) {
  // Canvas dimensions for collision
  int canvas_width = engine.get_width();
  int canvas_height = engine.get_height();

  // Iterate through all possible entity IDs (simple naive loop)
  // A better ECS would allow iterating only active entities with specific
  // components. Since we don't have an iterator yet, we'll just check a safe
  // range of IDs. Assuming we didn't create more than 100 entities for now.
  for (EntityID id = 0; id < 100; id++) {
    DisplaceableComponent *d = registry.get_displaceable(id);
    DrawableComponent *draw_ref = registry.get_drawable_ref(id);

    if (d && draw_ref && draw_ref->type == DrawableType::FOREGROUND) {
      // 1. Update Physics
      d->x += d->vx;
      d->y += d->vy;

      // Access actual sprite size for collision
      // We need to resolve the pointer to the drawable in the engine
      ForegroundDrawable *fd =
          engine.get_foreground_drawable(draw_ref->drawable_index);
      if (!fd || !fd->sprite)
        continue;

      int width = fd->sprite->width;
      int height = fd->sprite->height;

      // Collision Logic (Simple bouncing)
      if (d->x < 0) {
        d->x = 0;
        d->vx = -d->vx;
        on_bounce(engine);
      } else if (d->x + width > canvas_width) {
        d->x = canvas_width - width;
        d->vx = -d->vx;
        on_bounce(engine);
      }

      if (d->y < 0) {
        d->y = 0;
        d->vy = -d->vy;
        on_bounce(engine);
      } else if (d->y + height > canvas_height) {
        d->y = canvas_height - height;
        d->vy = -d->vy;
        on_bounce(engine);
      }

      // 2. Sync to Drawable
      fd->x = (int)d->x;
      fd->y = (int)d->y;
    }
  }
}