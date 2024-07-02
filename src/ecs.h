#pragma once

#include <stdint.h>

typedef uint64_t Entity;

typedef enum ComponentType {
  COMPONENT_TRANSFORM,
  COMPONENT_MODEL,
  COMPONENT_PLAYER,
  COMPONENT_ENEMY_FLOATER,
  COMPONENT_MAX
} ComponentType;

// Holds entity ID and component data. It stores the entity ID so that it is
// easily accessible when iterating.
typedef struct EntityComponent {
  Entity entity;
  // Component data. This is aligned to 16 bytes so that SIMD can be used.
  _Alignas(16) char component[];
} EntityComponent;

void ecs_register_component(ComponentType type, int data_size_bytes);

Entity entity_create();
void entity_destroy(Entity e);

void *entity_add_component(Entity e, ComponentType type);
void *entity_get_component(Entity e, ComponentType type);
void *entity_get_component_or_null(Entity e, ComponentType type);
void entity_remove_component(Entity e, ComponentType type);

EntityComponent *component_begin(ComponentType type);
EntityComponent *component_next(ComponentType type, EntityComponent *ec);
EntityComponent *component_end(ComponentType type);
