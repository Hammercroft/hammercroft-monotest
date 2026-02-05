#ifndef IGAME_H
#define IGAME_H

namespace mtengine {

class Engine;

/**
@class IGame
@brief The interface / base abstract class representing a game.
*/
class IGame {
public:
  virtual ~IGame() = default;

  /**
  @brief The game's initialization logic, which runs before the main game loop
  starts.
  */
  virtual void init(Engine &context) = 0;

  /**
  @brief The game's "early update" logic. Runs before ECS processing in the main
  game loop, which makes it a good place to handle inputs.
  @param context The engine context.
  @param dt The time elapsed since the last frame.
  */
  virtual void early_update(Engine &context, float dt) = 0;

  /**
  @brief Update logic for the main game loop, which runs after all ECS
  components (bar drawables) have been processed during an iteration of the main
  game loop.
  @param context The engine context.
  @param dt The time elapsed since the last frame.
  */
  virtual void update(Engine &context, float dt) = 0;

  /**
  @brief Logic that runs for when a graceful exit is requested; intended for
         displaying a quit prompt.
  @note It is not guaranteed that this function will stop the game.
  */
  virtual void stop(Engine &context) = 0;
};

// Factory function types
typedef IGame *(*CreateGameFunc)();
typedef void (*DestroyGameFunc)(IGame *);

} // namespace mtengine

extern "C" {
mtengine::IGame *create_game();
void destroy_game(mtengine::IGame *game);
}

#endif // IGAME_H