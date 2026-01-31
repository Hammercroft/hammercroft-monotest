#include "engine/engine.h"
#include "game.h"
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#endif

int main(int /*argc*/, char ** /*argv*/) {
  // Set CWD to executable directory
#ifdef _WIN32
  char exe_path[PATH_MAX];
  DWORD count = GetModuleFileNameA(NULL, exe_path, PATH_MAX);
  if (count != 0 && count < PATH_MAX) {
    // Find last backslash to get directory
    char *last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
      *last_slash = '\0'; // Truncate to directory
      if (_chdir(exe_path) != 0) {
        std::cerr << "Failed to change directory to executable path"
                  << std::endl;
      }
    }
  }
#else
  char exe_path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX);
  if (count != -1) {
    exe_path[count] = '\0'; // Null-terminate
    if (chdir(dirname(exe_path)) != 0) {
      std::cerr << "Failed to change directory to executable path" << std::endl;
    }
  }
#endif

  Engine *engine = create_engine();
  if (!engine) {
    std::cerr << "Failed to create engine" << std::endl;
    return 1;
  }

  if (!engine->init(CANVAS_WIDTH, CANVAS_HEIGHT, SCALE)) {
    std::cerr << "Failed to initialize engine" << std::endl;
    delete engine;
    return 1;
  }

  Game game;
  game.init(*engine);

  // Start loop
  engine->start(game);

  delete engine;
  return 0;
}
