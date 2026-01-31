#ifndef ECS_H
#define ECS_H

#include <functional>
#include <stdint.h>
#include <vector>

// Simple Entity ID
typedef uint32_t EntityID;

enum class DrawableType { NONE = 0, BACKGROUND, WORLD, FOREGROUND };

struct DrawableComponent {
  DrawableType type;
  int drawable_index; // Index into the Engine's contiguous array
};

struct DisplaceableComponent {
  bool active;
  float x, y;
  float vx, vy;
};

class Registry {
public:
  Registry();
  ~Registry();

  // Entity Management
  EntityID create_entity();
  void destroy_entity(EntityID id);

  // Component Access (Simple array-of-structs or similar for now)
  // For this task, we focus on storing the link to drawables.
  // In a full ECS, this would be sparse sets or archetypes.
  // We will use a simple vector indexed by EntityID for now.

  // Set the drawable info for an entity
  void set_drawable_ref(EntityID id, DrawableType type, int index);

  // Get the drawable info
  DrawableComponent *get_drawable_ref(EntityID id);

  // Displaceable Components
  void set_displaceable(EntityID id, float x, float y, float vx, float vy);
  DisplaceableComponent *get_displaceable(EntityID id);

  // Call this when the Engine moves a drawable in memory
  void update_drawable_index(EntityID owner_id, int new_index);

private:
  struct EntityData {
    bool active;
    DrawableComponent drawable;
    DisplaceableComponent displaceable;
  };

  std::vector<EntityData> entities;
  std::vector<EntityID> free_ids;
};

#endif // ECS_H
