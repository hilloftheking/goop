#pragma once

#include <stdint.h>

typedef uint64_t Entity;

typedef enum ComponentType {
  COMPONENT_MODEL,
  COMPONENT_ENEMY,
  COMPONENT_MAX
} ComponentType;

// Holds entity ID and component data. It stores the entity ID so that it is
// easily accessible when iterating.
typedef struct EntityComponent {
  Entity entity;
  char component[];
} EntityComponent;

void ecs_register_component(ComponentType type, int data_size_bytes);

Entity entity_create();

void *entity_add_component(Entity e, ComponentType type);
void *entity_get_component(Entity e, ComponentType type);
void *entity_get_component_or_null(Entity e, ComponentType type);
void entity_remove_component(Entity e, ComponentType type);

EntityComponent *component_begin(ComponentType type);
EntityComponent *component_next(ComponentType type, EntityComponent *ec);
EntityComponent *component_end(ComponentType type);
