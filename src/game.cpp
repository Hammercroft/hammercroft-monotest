#include "game.h"
#include "engine/engine.h"
#include <iostream>

// GAME IMPLEMENTATION, SOURCE

void Game::init(mtengine::Engine &context) {
  mtengine::BkgImage *bkg =
      context.load_bkg_image("assets/bkg/testbackground.pbm");
  context.try_set_active_background(bkg);
  std::cout << "game::init() called / game initialized\n";
}

void Game::early_update(mtengine::Engine &context, float dt) {
  // std::cout << "Early update\n";
}

void Game::update(mtengine::Engine &context, float dt) {
  // std::cout << "Update\n";
}

void Game::stop(mtengine::Engine &context) {
  std::cout << "game::stop() called\n";
}
