#include "scripting.h"
#include "ecs.h"
#include "engine.h"

// Include Lua C headers
extern "C" {
#include "../vendor/lua/src/lauxlib.h"
#include "../vendor/lua/src/lua.h"
#include "../vendor/lua/src/lualib.h"
}

// Game & Asset Includes
#include "../game.h" // Access to Game struct
#include "bkgimageassetentry.h"
#include "bkgimagefileloader.h"

#include <iostream>

ScriptManager::ScriptManager()
    : L(nullptr), engine_ref(nullptr), registry_ref(nullptr) {}

ScriptManager::~ScriptManager() { shutdown(); }

// Redefinitions for static linkage
// Note: We're keeping the one that sets the global pointer below.
// Actually, let's just keep ONE definition.
// Removing the one at the top.

void ScriptManager::shutdown() {
  if (L) {
    lua_close(L);
    L = nullptr;
  }
}

bool ScriptManager::load_script(const std::string &filepath) {
  current_script = filepath;
  if (luaL_loadfile(L, filepath.c_str()) != 0) {
    std::cerr << "Failed to load script: " << filepath << "\n"
              << lua_tostring(L, -1) << std::endl;
    lua_pop(L, 1);
    return false;
  }
  return true;
}

void ScriptManager::run_script() {
  if (!L)
    return;

  // The script chunk is already at the top of the stack if load_script
  // succeeded. However, if we called load_script separately, we might need to
  // handle this differently. For now, let's assume load_script pushes the
  // chunk, and run_script executes it. Actually, safe pattern: reload and run.

  if (luaL_dofile(L, current_script.c_str()) != 0) {
    std::cerr << "Runtime error in script: " << current_script << "\n"
              << lua_tostring(L, -1) << std::endl;
  } else {
    std::cout << "Script " << current_script << " executed successfully."
              << std::endl;
  }
}

void ScriptManager::reload() {
  std::cout << "Reloading script..." << std::endl;
  run_script();
}

// ================= Bindings ================= //

void ScriptManager::register_bindings() {
  // We'll put our functions in a global "Engine" table
  lua_newtable(L);

  lua_pushlightuserdata(L, this);
  lua_setfield(L, -2, "__ptr"); // hidden pointer to self for static callbacks

  lua_pushcclosure(L, lua_CreateEntity, 0);
  lua_setfield(L, -2, "CreateEntity");

  lua_pushcclosure(L, lua_SetSprite, 0);
  lua_setfield(L, -2, "SetSprite");

  lua_pushcclosure(L, lua_SetPosition, 0);
  lua_setfield(L, -2, "SetPosition");

  lua_pushcclosure(L, lua_SetVelocity, 0);
  lua_setfield(L, -2, "SetVelocity");

  lua_pushcclosure(L, lua_PlaySound, 0);
  lua_setfield(L, -2, "PlaySound");

  lua_pushcclosure(L, lua_GetTime, 0);
  lua_setfield(L, -2, "GetTime");

  lua_pushcclosure(L, lua_SetBackgroundImage, 0);
  lua_setfield(L, -2, "SetBackgroundImage");

  lua_setglobal(L, "Engine");
}

// Helper to get ScriptManager instance if needed, but for now we rely on global
// state or closures if we want to be capturing. Actually, since these are
// static C functions, we need access to the Engine/Registry. The common pattern
// is to explicitly pass context or use a singleton if we are lazy. Since we are
// inside member functions, we can access members... wait, no, these are static.
// Let's use a dirty static pointer hack for this proof of concept or upvalue.
// Better: Helper function to extract ScriptManager from registry or upvalue?
// For simplicity in this step, let's assume ONE active ScriptManager and use a
// static pointer. If multiple managers are needed, we'd pass 'this' as an
// upvalue to the closure.

static ScriptManager *g_ScriptManager = nullptr;

// Re-implement init to set global
bool ScriptManager::init(Game *game, Engine *engine, Registry *registry) {
  g_ScriptManager = this;
  game_ref = game;
  engine_ref = engine;
  registry_ref = registry;

  L = luaL_newstate();
  if (!L)
    return false;

  // Sandbox: Only load safe libraries
  // 1. Base (print, pairs, etc.)
  lua_pushcfunction(L, luaopen_base);
  lua_pushstring(L, "");
  lua_call(L, 1, 0);

  // 2. Table
  lua_pushcfunction(L, luaopen_table);
  lua_pushstring(L, LUA_TABLIBNAME);
  lua_call(L, 1, 0);

  // 3. String
  lua_pushcfunction(L, luaopen_string);
  lua_pushstring(L, LUA_STRLIBNAME);
  lua_call(L, 1, 0);

  // 4. Math
  lua_pushcfunction(L, luaopen_math);
  lua_pushstring(L, LUA_MATHLIBNAME);
  lua_call(L, 1, 0);

  // 5. Remove dangerous globals from Base lib
  lua_pushnil(L);
  lua_setglobal(L, "dofile");
  lua_pushnil(L);
  lua_setglobal(L, "loadfile");
  lua_pushnil(L);
  lua_setglobal(L, "load"); // 'load' in 5.2+, 'loadstring' in 5.1?
  // In 5.1 'loadstring' is available. 'load' operates on function.
  // We probably want to keep loadstring for simple logic but it can compile
  // byte code? Let's remove loadfile definitely.

  // Note: luaL_openlibs(L) removed.
  register_bindings();
  return true;
}

// Redefinitions for static linkage
int ScriptManager::lua_CreateEntity(lua_State *L) {
  if (!g_ScriptManager)
    return 0;
  EntityID id = g_ScriptManager->registry_ref->create_entity();
  lua_pushinteger(L, (int)id);
  return 1;
}

int ScriptManager::lua_SetSprite(lua_State *L) {
  if (!g_ScriptManager)
    return 0;
  int id = luaL_checkinteger(L, 1);
  const char *spriteName = luaL_checkstring(L, 2);

  // We need to resolve the Sprite* from the name.
  // game.cpp has the sprite_table. Engine doesn't own it directly?
  // Engine owns drawables but not the asset table directly in current
  // architecture? Checking game.cpp... game.cpp has `sprite_table`. Use Case:
  // We might need to look up assets via Engine if we move asset manager there.
  // OPTION: Pass sprite_table to ScriptManager or expose a getter on Engine?
  // For now, let's assume we can't look it up yet without a change.
  // Wait, Engine doesn't have `get_sprite(name)`.
  // FIXME: We need a way to look up sprites.

  // Temporary Hack: Just log it to prove binding works, or fail gracefully.
  std::cout << "[Lua] SetSprite(" << id << ", " << spriteName
            << ") - Asset Lookup Not Implemented Yet" << std::endl;
  return 0;
}

int ScriptManager::lua_SetPosition(lua_State *L) {
  if (!g_ScriptManager)
    return 0;
  int id = luaL_checkinteger(L, 1);
  double x = luaL_checknumber(L, 2);
  double y = luaL_checknumber(L, 3);

  // Update Displaceable
  DisplaceableComponent *d =
      g_ScriptManager->registry_ref->get_displaceable(id);
  // If not exists, we might need to create it?
  // Registry::set_displaceable creates or updates.
  // We need current vx, vy to preserve them?
  float vx = 0, vy = 0;
  if (d) {
    vx = d->vx;
    vy = d->vy;
  }

  g_ScriptManager->registry_ref->set_displaceable(id, (float)x, (float)y, vx,
                                                  vy);

  // Also update Drawable if exists? The main loop does sync.
  return 0;
}

int ScriptManager::lua_SetVelocity(lua_State *L) {
  if (!g_ScriptManager)
    return 0;
  int id = luaL_checkinteger(L, 1);
  double vx = luaL_checknumber(L, 2);
  double vy = luaL_checknumber(L, 3);

  DisplaceableComponent *d =
      g_ScriptManager->registry_ref->get_displaceable(id);
  float x_val = 0, y_val = 0;
  if (d) {
    x_val = d->x;
    y_val = d->y;
  }

  g_ScriptManager->registry_ref->set_displaceable(id, x_val, y_val, (float)vx,
                                                  (float)vy);
  return 0;
}

int ScriptManager::lua_PlaySound(lua_State *L) {
  if (!g_ScriptManager)
    return 0;
  const char *path = luaL_checkstring(L, 1);
  g_ScriptManager->engine_ref->play_sound(path);
  return 0;
}

int ScriptManager::lua_GetTime(lua_State *L) {
  if (!g_ScriptManager)
    return 0;

  // Return time in seconds (float) for easier game logic, or ms?
  // Let's stick to seconds for logic usually.
  unsigned long ms = g_ScriptManager->engine_ref->get_time_ms();
  lua_pushnumber(L, (double)ms / 1000.0);
  return 1;
}

int ScriptManager::lua_SetBackgroundImage(lua_State *L) {
  if (!g_ScriptManager || !g_ScriptManager->game_ref)
    return 0;

  const char *path = luaL_checkstring(L, 1);
  Game *game = g_ScriptManager->game_ref;

  // 1. Check if already loaded
  BkgImage *bkg = GetBkgImage(game->bkg_table, BKG_TABLE_SIZE, path);

  // 2. If not, load it
  if (!bkg) {
    // We strictly use the path as the "name" for now
    std::cout << "[Lua] Loading background: " << path << std::endl;
    bkg = LoadBkgImagePBM(&game->bkg_arena, path);
    if (!bkg) {
      luaL_error(L, "Failed to load background image: %s", path);
      return 0;
    }
    RegisterBkgImageAsAsset(game->bkg_table, BKG_TABLE_SIZE, path, bkg);
  }

  // 3. Set Active
  g_ScriptManager->engine_ref->set_active_background(bkg);

  return 0;
}
