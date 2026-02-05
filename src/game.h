#ifndef GAME_H
#define GAME_H

#include "engine/bkg/bkgimage.h"
#include "engine/igame.h"
#include "gameinfo.h"
#include <cstddef>

// GAME IMPLEMENTATION, HEADER

// Game implementation class
class Game : public mtengine::IGame {

public:
  Game() = default;
  ~Game() = default;

  void init(mtengine::Engine &context) override;
  void early_update(mtengine::Engine &context, float dt) override;
  void update(mtengine::Engine &context, float dt) override;
  void stop(mtengine::Engine &context) override;
};

#endif // GAME_H