#include "engine.h"
#include "ecs.h"
#include <iostream>

void Engine::set_registry(Registry *reg) { registry = reg; }

int Engine::add_world_drawable(WorldDrawable &d) {
  if (world_drawables_count >= MAX_WORLD_DRAWABLES) {
    std::cerr << "Engine Error: World drawable limit reached!" << std::endl;
    return -1;
  }

  int index = world_drawables_count;
  world_drawables[index] = d;
  world_drawables_count++;
  return index;
}

void Engine::remove_world_drawable(int index) {
  if (index < 0 || index >= world_drawables_count) {
    return;
  }

  int last_index = world_drawables_count - 1;

  // If we are not removing the very last element, we do the swap
  if (index != last_index) {
    // Move the last element into the gap
    world_drawables[index] = world_drawables[last_index];

    // Notify ECS that this entity's drawable has moved
    if (registry) {
      uint32_t moved_owner_id = world_drawables[index].owner_id;
      registry->update_drawable_index(moved_owner_id, index);
    }
  }

  // Decrement count (effectively popping the last element)
  world_drawables_count--;
}

int Engine::add_foreground_drawable(ForegroundDrawable &d) {
  if (foreground_drawables_count >= MAX_FOREGROUND_DRAWABLES) {
    std::cerr << "Engine Error: Foreground drawable limit reached!"
              << std::endl;
    return -1;
  }

  int index = foreground_drawables_count;
  foreground_drawables[index] = d;
  foreground_drawables_count++;
  return index;
}

void Engine::remove_foreground_drawable(int index) {
  if (index < 0 || index >= foreground_drawables_count) {
    return;
  }

  int last_index = foreground_drawables_count - 1;

  // If we are not removing the very last element, we do the swap
  if (index != last_index) {
    // Move the last element into the gap
    foreground_drawables[index] = foreground_drawables[last_index];

    // Notify ECS that this entity's drawable has moved
    if (registry) {
      uint32_t moved_owner_id = foreground_drawables[index].owner_id;
      registry->update_drawable_index(moved_owner_id, index);
    }
  }

  // Decrement count
  foreground_drawables_count--;
}

void Engine::toggle_interlace() {
  interlaced_mode = !interlaced_mode;
  std::cout << "Interlaced Mode: " << (interlaced_mode ? "ON" : "OFF")
            << std::endl;
}

void Engine::set_interlace(bool active) { interlaced_mode = active; }

void Engine::toggle_invert_colors() {
  invert_colors = !invert_colors;
  std::cout << "Invert Colors: " << (invert_colors ? "ON" : "OFF") << std::endl;
}

void Engine::set_invert_colors(bool active) { invert_colors = active; }

void Engine::toggle_dead_space_color() {
  dead_space_white = !dead_space_white;
  std::cout << "Dead Space Color: " << (dead_space_white ? "WHITE" : "BLACK")
            << std::endl;
}

void Engine::set_dead_space_color(bool white) { dead_space_white = white; }

void Engine::toggle_pixel_perfect() {
  pixel_perfect_mode = !pixel_perfect_mode;
  std::cout << "Pixel Perfect: " << (pixel_perfect_mode ? "ON" : "OFF")
            << std::endl;
}

void Engine::set_pixel_perfect(bool active) { pixel_perfect_mode = active; }
