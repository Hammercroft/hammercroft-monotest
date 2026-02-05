#ifndef GAME_H
#define GAME_H

#include "engine/bkgimagearena.h"
#include "engine/bkgimageassetentry.h"
#include "engine/ecs.h"
#include "engine/scripting.h"
#include "engine/spritearena.h"
#include "engine/spriteassetentry.h"
#include <vector>

const int CANVAS_WIDTH = 480;
const int CANVAS_HEIGHT = 320;
const int SCALE = 2;

const size_t SPRITE_ARENA_SIZE = 32 * 1024 * 1024; // 32 MB
const size_t BKG_ARENA_SIZE = 8 * 1024 * 1024;     // 8 MB

#define SPRITE_TABLE_SIZE 4096
#define BKG_TABLE_SIZE 128

class Engine;

struct Game {
  // Memory arenas

  SpriteArena sprite_arena;
  BkgImageArena bkg_arena;

  // ECS Registry
  Registry registry;

  // Asset Lookup Tables

  SpriteAssetEntry sprite_table[SPRITE_TABLE_SIZE];
  BkgImageAssetEntry bkg_table[BKG_TABLE_SIZE];

  // Scripting Engine
  ScriptManager scripting;

  void init(Engine &engine);
  void pre_ecs_update(Engine &engine);
  void ecs_update(Engine &engine);
  void post_ecs_update(Engine &engine);
};

#endif // GAME_H
