#include "ecs.h"
#include "fixed_array.h"
#include "int_map.h"

typedef struct RegisteredComponent {
  // Stores all EntityComponents contiguously
  FixedArray components;
  // Maps entities to component indices
  IntMap entity_to_component;
} RegisteredComponent;

static RegisteredComponent registered_components[COMPONENT_MAX];

#define MAX_COMPONENTS 64

void ecs_register_component(ComponentType type, int data_size_bytes) {
  RegisteredComponent *rc = &registered_components[type];
  fixed_array_create(&rc->components, sizeof(EntityComponent) + data_size_bytes,
                     MAX_COMPONENTS);
  int_map_create(&rc->entity_to_component);
}

Entity entity_create() {
  static uint64_t id = 0;
  return id++;
}

void *entity_add_component(Entity e, ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  int_map_insert(&rc->entity_to_component, e, rc->components.count);
  EntityComponent *ec = fixed_array_append(&rc->components, NULL);
  ec->entity = e;
  return ec->component;
}

void *entity_get_component(Entity e, ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  uint64_t *p_idx = int_map_get(&rc->entity_to_component, e);
  if (!p_idx)
    return NULL;
  EntityComponent *ec = fixed_array_get(&rc->components, (int)*p_idx);
  return ec->component;
}

EntityComponent *component_begin(ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  return rc->components.data;
}

EntityComponent *component_next(ComponentType type, EntityComponent *ec) {
  RegisteredComponent *rc = &registered_components[type];
  return (char *)ec + rc->components.element_size;
}

EntityComponent *component_end(ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  return fixed_array_get(&rc->components, rc->components.count);
}