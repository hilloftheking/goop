#include <stdio.h>

#include "core.h"
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

#define MAX_COMPONENTS 128

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

void entity_destroy(Entity e) {
  for (int i = 0; i < COMPONENT_MAX; i++) {
    if (entity_get_component_or_null(e, i)) {
      entity_remove_component(e, i);
    }
  }
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
  if (!p_idx) {
    fprintf(stderr, "Entity %llu does not have component %d\n", e, type);
    exit_fatal_error();
    return NULL;
  }
  EntityComponent *ec = fixed_array_get(&rc->components, (int)*p_idx);
  return ec->component;
}

void *entity_get_component_or_null(Entity e, ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  uint64_t *p_idx = int_map_get(&rc->entity_to_component, e);
  if (!p_idx) {
    return NULL;
  }
  EntityComponent *ec = fixed_array_get(&rc->components, (int)*p_idx);
  return ec->component;
}

void entity_remove_component(Entity e, ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  uint64_t *p_idx = int_map_get(&rc->entity_to_component, e);
  if (!p_idx)
    return;
  uint64_t idx = *p_idx;
  fixed_array_remove(&rc->components, (int)idx);
  int_map_remove(&rc->entity_to_component, e);

  // Move component indices to the left
  for (uint64_t i = 0; i < rc->entity_to_component.capacity; i++) {
    IntMapKV *kv = &rc->entity_to_component.data[i];
    if (kv->value > idx) {
      kv->value--;
    }
  }
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

int component_get_count(ComponentType type) {
  RegisteredComponent *rc = &registered_components[type];
  return rc->components.count;
}

EntityComponent *component_get_from_idx(ComponentType type, int idx) {
  RegisteredComponent *rc = &registered_components[type];
  return fixed_array_get(&rc->components, idx);
}
