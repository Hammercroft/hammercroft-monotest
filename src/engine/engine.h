#ifndef ENGINE_H
#define ENGINE_H

#include "bkg/bkgimagemanager.h"
#include <iostream>

// ENGINE, HEADER

// Static limits regardless of memory
#define BKG_TABLE_SIZE 32
#define SPRITE_TABLE_SIZE 2048

/**
@namespace mtengine
@brief The namespace used for all MONOTEST engine code.
*/
namespace mtengine {

class IGame;

/**
@class Engine
@brief The main engine singleton.
*/
class Engine {

  // Engine state
  bool main_game_loop_running = true;
  long tick = 0;
  BkgImage *active_background = nullptr;

  // Presentation Flags
  bool pixel_perfect_mode = true;
  bool invert_colors = false;
  bool dead_space_is_white = true;

  BkgImageManager bkg_manager{0}; // to be initialized by .init()

public:
  // destructor, implemented on platform specific files
  ~Engine();

  // not a method, but this allows engine.cpp to access protected constructor
  friend Engine *singleton();

  /**
  @brief Initializes the engine.
  @param width The width of the canvas.
  @param height The height of the canvas.
  @param scale The scale of the canvas.
  @param sprite_mem_size The size of the sprite memory arena.
  @param bkg_mem_size The size of the background image memory arena.
  @return True if the engine was initialized successfully, false otherwise.
  */
  virtual bool init(int width, int height, float scale, size_t sprite_mem_size,
                    size_t bkg_mem_size); // to be implemented on platform-side

  /**
  @brief Starts a game and plays it until exited.
  */
  virtual void play(IGame &game); // implemented on engine.cpp

  /**
    @brief Resets all "visual data" stored in the engine (e.g. for a scene
    transition). Unloads all assets, clears draw lists and lookup tables, and
    destroys all Engine-managed ECS Drawable components.
    @note You may need to re-assign ECS Entities their Drawables after calling
    this function.
    */
  virtual void clear_scene(); // implemented on engine.cpp

  /**
  @brief Unloads all assets and performs a full state reset by destroying
  all ECS Entities handled by the engine and their associated components.
  */
  virtual void unload_all(); // implemented on engine.cpp

  /**
  @brief Loads a background image from a file. The loaded image will also be
  listed in the background image asset lookup table with the filename as its
  key.
  @param filename The filename of the background image.
  @return The loaded background image.
  */
  virtual BkgImage *
  load_bkg_image(const char *filename); // implemented on engine.cpp

  /**
  @brief Sets the active background image.
  @param bkg Pointer to the background image to set.
  */
  void set_active_background(BkgImage *bkg); // implemented on engine.cpp

  /**
  @brief Safely sets the active background image.
  @param bkg Pointer to the background image to set.
  @return True if bkg is not null, false otherwise.
  */
  bool try_set_active_background(BkgImage *bkg); // implemented on engine.cpp

  /**
  @brief Gets the currently active background image.
  @return Pointer to the active background image, or nullptr if none is set.
  */
  BkgImage *get_active_background(); // implemented on engine.cpp

  // --- Presentation State Accessors ---

  // NO API TO BE PROVIDED FOR PIXEL PERFECT MODE!

  /**
  @brief Sets the global color inversion mode.
  @param enabled If true, the canvas colors are inverted (black becomes white).
  */
  void set_invert_colors(bool enabled);

  /**
  @brief Checks if color inversion is enabled.
  @return True if enabled, false otherwise.
  */
  bool get_invert_colors();

  /**
  @brief Sets the color of the "dead space" (borders) outside the canvas.
  @param is_white If true, dead space is white. If false, it is black.
  */
  void set_dead_space_is_white(bool is_white);

  /**
  @brief Checks if dead space is white.
  @return True if dead space is white, false if black.
  */
  bool get_dead_space_is_white();

protected:
  // constructor, to be implemented on platform-side
  Engine();

  // Poll platform-specific events (input, window close, etc.)
  // to be implemented on platform-side
  void poll_events();

  // Prepare the canvas for drawing
  // to be implemented on platform-side
  virtual void draw_prepare();

  // Draw all drawables to the canvas
  // to be implemented on platform-side
  virtual void draw_lists();

  // Present the canvas to the screen
  // to be implemented on platform-side
  virtual void draw_present();

private:
  // Common code for all platforms to prepare for asset management
  // implemented on engine.cpp
  void init_asset_management(size_t sprite_mem_size, size_t bkg_mem_size);
};

/**
@brief Returns the engine singleton, creating it if it doesn't exist.
*/
Engine *singleton();

} // namespace mtengine

#endif // ENGINE_H