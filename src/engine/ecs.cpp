#include "ecs.h"
#include <iostream>

Registry::Registry() {
  // Reserve index 0 as null/invalid if desired, but 0 is fine for now.
}

Registry::~Registry() {}

EntityID Registry::create_entity() {
  if (!free_ids.empty()) {
    EntityID id = free_ids.back();
    free_ids.pop_back();
    entities[id].active = true;
    entities[id].drawable = {DrawableType::NONE, -1};
    entities[id].displaceable = {false, 0, 0, 0, 0};
    return id;
  }

  EntityID id = (EntityID)entities.size();
  entities.push_back({true, {DrawableType::NONE, -1}, {false, 0, 0, 0, 0}});
  return id;
}

void Registry::destroy_entity(EntityID id) {
  if (id < entities.size() && entities[id].active) {
    entities[id].active = false;
    entities[id].drawable = {DrawableType::NONE, -1};
    entities[id].displaceable = {false, 0, 0, 0, 0};
    free_ids.push_back(id);
  }
}

void Registry::set_drawable_ref(EntityID id, DrawableType type, int index) {
  if (id < entities.size() && entities[id].active) {
    entities[id].drawable.type = type;
    entities[id].drawable.drawable_index = index;
  }
}

DrawableComponent *Registry::get_drawable_ref(EntityID id) {
  if (id < entities.size() && entities[id].active) {
    // Return null if no drawable? Or just the component.
    // If type is NONE, it's effectively null.
    if (entities[id].drawable.type == DrawableType::NONE)
      return nullptr;
    return &entities[id].drawable;
  }
  return nullptr;
}

DisplaceableComponent *Registry::get_displaceable(EntityID id) {
  if (id >= entities.size() || !entities[id].active) {
    return nullptr;
  }
  if (!entities[id].displaceable.active) {
    return nullptr;
  }
  return &entities[id].displaceable;
}

void Registry::set_displaceable(EntityID id, float x, float y, float vx,
                                float vy) {
  if (id >= entities.size() || !entities[id].active) {
    return;
  }
  entities[id].displaceable.active = true;
  entities[id].displaceable.x = x;
  entities[id].displaceable.y = y;
  entities[id].displaceable.vx = vx;
  entities[id].displaceable.vy = vy;
}

void Registry::update_drawable_index(EntityID owner_id, int new_index) {
  if (owner_id < entities.size() && entities[owner_id].active) {
    // Debug check: ensure we are updating the right thing?
    // For now, blindly update.
    entities[owner_id].drawable.drawable_index = new_index;
  } else {
    std::cerr << "ECS Error: specific owner_id " << owner_id
              << " not found or inactive during swap-update." << std::endl;
  }
}
