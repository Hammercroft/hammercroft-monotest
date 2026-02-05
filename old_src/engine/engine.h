#ifndef ENGINE_H
#define ENGINE_H

#include "drawables.h"
#include <functional>
#include <string>
#include <unistd.h>

class Registry; // Forward declaration

class Engine {
public:
  virtual ~Engine() {}

  // Registry for ECS updates
  void set_registry(Registry *reg);
  Registry *registry = nullptr;

  // Global rendering state
  bool invert_colors = false;
  bool interlaced_mode = false;
  bool dead_space_white = true;
  bool pixel_perfect_mode = true;

  // Feature Toggles (API)
  void toggle_interlace();
  void set_interlace(bool active);

  void toggle_invert_colors();
  void set_invert_colors(bool active);

  void toggle_dead_space_color();
  void set_dead_space_color(bool white);

  void toggle_pixel_perfect();
  void set_pixel_perfect(bool active);

  // Drawable Management (World/Layer 2)
  // Returns the index of the added drawable
  int add_world_drawable(struct WorldDrawable &d);

  // Removes drawable at index, performing swap-and-pop
  void remove_world_drawable(int index);

  // Drawable Management (Foreground/Layer 3)
  int add_foreground_drawable(struct ForegroundDrawable &d);
  void remove_foreground_drawable(int index);

  ForegroundDrawable *get_foreground_drawable(int index) {
    if (index < 0 || index >= foreground_drawables_count)
      return nullptr;
    return &foreground_drawables[index];
  }

  // Initialize the engine with specific dimensions
  virtual bool init(int width, int height, int scale) = 0;

  // The three layers of Drawables
  static const int MAX_BACKGROUND_DRAWABLES = 64;
  static const int MAX_WORLD_DRAWABLES = 256;
  static const int MAX_FOREGROUND_DRAWABLES = 128;

  struct BackgroundDrawable background_drawables[MAX_BACKGROUND_DRAWABLES];
  int background_drawables_count = 0;

  struct WorldDrawable world_drawables[MAX_WORLD_DRAWABLES];
  int world_drawables_count = 0;

  struct ForegroundDrawable foreground_drawables[MAX_FOREGROUND_DRAWABLES];
  int foreground_drawables_count = 0;

  // TODO make an on-demand sort function for BackgroundDrawables and
  // ForegroundDrawables that sorts by z-index (sort key). DO NOT INCLUDE THE
  // SORT IN THE RENDERING LOOP!
  // TODO find a way to track specific drawables as they risk being moved due to
  // sorting

  // Process pending events (input, window resize, close)
  // Returns false if the application should quit
  virtual bool process_events() = 0;

  // Start the engine with a specific game instance
  // Template prevents circular dependency on Game type
  template <typename GameApp> void start(GameApp &game) {
    run_loop([&]() {
      game.pre_ecs_update(*this);
      ecs_process();
      game.post_ecs_update(*this);
      draw_start();
      draw_lists();
      draw_end();
    });
  }

  // ECS Processing in update loop
  virtual void ecs_process() = 0;

  // Rendering steps
  virtual void draw_start() = 0; // prepare canvas & empty draw queue
  virtual void draw_lists() = 0; // draw from drawables to canvas
  virtual void draw_end() = 0;   // present canvas to application window

  // Get current state
  virtual bool is_running() = 0;
  virtual unsigned long get_time_ms() = 0;
  virtual void sleep_ms(int ms) = 0;

  // Audio configuration
  virtual void play_sound(const char *filename) = 0;
  virtual void load_sound(const char *filename) = 0;
  virtual void clear_sounds() = 0;

  // Background Management
  virtual void set_active_background(struct BkgImage *bkg) = 0;
  virtual struct BkgImage *get_active_background() = 0;

  // Dimensions
  virtual int get_width() const = 0;
  virtual int get_height() const = 0;

protected:
  // Run the main game loop with a provided callback
  void run_loop(std::function<void()> loop_body) {
    while (process_events()) {
      loop_body();
      sleep_ms(16); // ~60 FPS simple cap
      // sleep_ms(20); // ~50 FPS simple cap
    }
  }
};

// Factory function to create the appropriate engine instance
Engine *create_engine();

#endif // ENGINE_H