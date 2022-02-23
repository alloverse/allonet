#define ALLO_INTERNALS 1
#include <allonet/state/state_read.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "alloverse_generated.h"
using namespace Alloverse;

extern "C" void allo_state_create_parsed(allo_state *state, void *buf, size_t len)
{
    // realloc = we can reuse the same buffer as last time, especially if the size is ~the same
    state->flat = realloc(state->flat, len);
    // copy over the data we need
    memcpy(state->flat, buf, len);
    state->flatlength = len;
    
    // parse a read-only version with the C API so API clients can use the state
    state->state = Alloverse_State_as_root(state->flat);
    state->revision = Alloverse_State_revision_get(state->state);

    // parse read-only with the C++ API so internal code can use easier APIs.
    state->_cur = GetState(state->flat);
}

extern "C" void allo_state_destroy(allo_state *state)
{
    free(state->flat);
}


void allo_generate_id(char *str, size_t len)
{
  for (size_t i = 0; i < len-1; i++)
  {
    str[i] = 'a' + rand() % 25;
  }
  str[len-1] = 0;
}

allo_entity* state_get_entity(allo_state* state, const char* entity_id)
{
  if (!state || !entity_id || strlen(entity_id) == 0)
    return NULL;
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    if (strcmp(entity_id, entity->id) == 0)
    {
      return entity;
    }
  }
  return NULL;
}

extern allo_entity* entity_get_parent(allo_state* state, allo_entity* entity)
{
  cJSON* relationships = cJSON_GetObjectItemCaseSensitive(entity->components, "relationships");
  cJSON* parentIdJ = cJSON_GetObjectItemCaseSensitive(relationships, "parent");
  if (!parentIdJ) return NULL;
  return state_get_entity(state, cJSON_GetStringValue(parentIdJ));
}

extern allo_m4x4 entity_get_transform(allo_entity* entity)
{
  if(!entity)
    return allo_m4x4_identity();
  cJSON* transform = cJSON_GetObjectItemCaseSensitive(entity->components, "transform");
  cJSON* matrix = cJSON_GetObjectItemCaseSensitive(transform, "matrix");
  if (!transform || !matrix || cJSON_GetArraySize(matrix) != 16)
    return allo_m4x4_identity();

  return cjson2m(matrix);
}

void entity_set_transform(allo_entity* entity, allo_m4x4 m)
{
  for (int i = 0; i < 16; i++)
  {
    assert(isnan(m.v[i]) == false);
  }
  cJSON* transform = cJSON_GetObjectItemCaseSensitive(entity->components, "transform");
  cJSON* matrix = cJSON_GetObjectItemCaseSensitive(transform, "matrix");
  if (!transform || !matrix || cJSON_GetArraySize(matrix) != 16) 
  {
    matrix = cJSON_CreateDoubleArray(m.v, 16);
    transform = cjson_create_object("matrix", matrix, NULL);
    cJSON_AddItemToObject(entity->components, "transform", transform);
  }
  else 
  {
    for (int i = 0; i < 16; i++)
    {
      cJSON_SetNumberValue(cJSON_GetArrayItem(matrix, i), m.v[i]);
    }
  }
}

allo_m4x4 entity_get_transform_in_coordinate_space(allo_state *state, allo_entity* entity, allo_entity* space)
{
  allo_m4x4 m = entity_get_transform(entity);
  return state_convert_coordinate_space(state, m, entity_get_parent(state, entity), space);
}

static allo_m4x4 entity_get_transform_to_world(allo_state* state, allo_entity *ent)
{
  if (!ent) {
    return allo_m4x4_identity();
  }
  allo_entity* parent = entity_get_parent(state, ent);
  allo_m4x4 my_transform = entity_get_transform(ent);
  if (parent) {
    return allo_m4x4_concat(entity_get_transform_to_world(state, parent), my_transform);
  }
  return my_transform;
}

allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* old, allo_entity* neww)
{
  allo_m4x4 worldFromOld = entity_get_transform_to_world(state, old);
  allo_m4x4 worldFromNew = entity_get_transform_to_world(state, neww);
  allo_m4x4 newFromWorld = allo_m4x4_inverse(worldFromNew);
  allo_m4x4 newFromOld = allo_m4x4_concat(newFromWorld, worldFromOld);

  return allo_m4x4_concat(newFromOld, m);
}
