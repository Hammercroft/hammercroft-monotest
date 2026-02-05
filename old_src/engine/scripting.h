#pragma once

#include <string>

// Forward declaration to avoid including lua headers in game.h
extern "C" {
struct lua_State;
}

class Engine;
class Registry;
struct Game; // Forward declaration

class ScriptManager {
public:
  ScriptManager();
  ~ScriptManager();

  bool init(Game *game, Engine *engine, Registry *registry);
  void shutdown();

  bool load_script(const std::string &filepath);
  void run_script();

  // Allow reloading for iteration
  void reload();

private:
  void register_bindings();

  // Bindings
  static int lua_CreateEntity(lua_State *L);
  static int lua_SetSprite(lua_State *L);
  static int lua_SetPosition(lua_State *L);
  static int lua_SetVelocity(lua_State *L);
  static int lua_PlaySound(lua_State *L);
  static int lua_GetTime(lua_State *L);
  static int lua_SetBackgroundImage(lua_State *L);

  lua_State *L = nullptr;
  Engine *engine_ref = nullptr;
  Registry *registry_ref = nullptr;
  Game *game_ref = nullptr;
  std::string current_script;
};
