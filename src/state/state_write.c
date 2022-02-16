#include <allonet/state/state_write.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

/** Here's my thinking with using flatbuffers for state:
 *  - Client receives a full worldstate and never creates/adds entities nor components
 *  - Server reserves memory for N entities
 *  - When server runs out of storage for entities, it doubles the served space and reallocates
 *  - Server compacts its flatbuffer before sending to clients
 *  - keep a flexbuffer for any non-conforming data
 * 
 * Potential issues:
 *  - How do I get flatbuffer to allocate a maximally-big buffer for each entity? it seems
 *    strings take a fixed size up-front e g. 
 */

void allo_state_reserve(allo_state *state, int entity_count);

void allo_state_init(allo_state *state)
{
    state->revision = 1;
    allo_state_reserve(state, 128);
}

void allo_state_reserve(allo_state *state, int entity_count)
{
    flatcc_builder_t builder, *B = &builder;
    flatcc_builder_init(&B);

    Alloverse_State_start_as_root(B);
    Alloverse_State_revision_add(B, state->revision);

    Alloverse_State_entities_start(B);

    for(int i = 0; i < entity_count; i++)
    {
        Alloverse_State_entities_push_start(B);
        Alloverse_Entity_id_create_str(B, "          ");

        Alloverse_Entity_components_start(B);
        // todo: fill entity with maximally allocated but empty components
        Alloverse_Entity_components_end(B);

        Alloverse_State_entities_push_end(B);
    }
    Alloverse_State2_entities_end(B);

    Alloverse_State_end_as_root(B);
  
    void *flatbuf = flatcc_builder_finalize_aligned_buffer(B, &state->flatlength);

    flatcc_builder_clear(B);
}

void allo_state_destroy(allo_state *state)
{
  allo_entity *entity = state->entities.lh_first;
  while(entity)
  {
    allo_entity *to_delete = entity;
    entity = entity->pointers.le_next;
    entity_destroy(state, to_delete);
  }
}


allo_entity *entity_create(allo_state *state, const char *id)
{
    allo_entity *entity = (allo_entity *)calloc(1, sizeof(allo_entity));
    entity->id = strdup(id);
    return entity;
}
void entity_destroy(allo_state *state, allo_entity *entity)
{
    cJSON_Delete(entity->components);
    free(entity->id);
    free(entity->owner_agent_id);
    free(entity);
}



allo_entity* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent)
{
  char generated_eid[11] = { 0 };
  allo_generate_id(generated_eid, 11);
  const char *eid = generated_eid;

  allo_entity* e = entity_create(state, eid);
  e->owner_agent_id = strdup(agent_id ? agent_id : "place");
  cJSON* children = cJSON_DetachItemFromObjectCaseSensitive(spec, "children");

  // components can be under ["components"] or just loose in the root dict
  cJSON *components = cJSON_DetachItemFromObjectCaseSensitive(spec, "components");
  if(components)
  {
    cJSON_Delete(spec);
  } else {
    components = spec;
  }
  e->components = components;

  if (parent)
  {
    cJSON* relationships = cjson_create_object("parent", cJSON_CreateString(parent), NULL);
    cJSON_AddItemToObject(spec, "relationships", relationships);
  }

  if (!cJSON_HasObjectItem(spec, "transform"))
  {
    cJSON* transform = cjson_create_object("matrix", m2cjson(allo_m4x4_identity()), NULL);
    cJSON_AddItemToObject(spec, "transform", transform);
  }

  LIST_INSERT_HEAD(&state->entities, e, pointers);


  cJSON* child = children ? children->child : NULL;
  while (child) 
  {
    cJSON* next = child->next;
    cJSON* spec = cJSON_DetachItemViaPointer(children, child);
    allo_state_add_entity_from_spec(state, agent_id, spec, eid);
    child = next;
  }
  cJSON_Delete(children);
  return e;
}

bool allo_state_remove_entity_id(allo_state *state, const char *eid, allo_removal_mode mode)
{
  allo_entity* removed_entity = state_get_entity(state, eid);
  if(!removed_entity)
  {
    return false;
  }
  return allo_state_remove_entity(state, removed_entity, mode);
}
bool allo_state_remove_entity(allo_state *state, allo_entity *removed_entity, allo_removal_mode mode)
{
  LIST_REMOVE(removed_entity, pointers);

  // remove or reparent children too
  arr_t(allo_entity*) children;
  arr_init(&children);
  allo_entity *entity = state->entities.lh_first;
  while(entity)
  {
    allo_entity *potential_child = entity;
    entity = entity->pointers.le_next;
    cJSON *relationships = cJSON_GetObjectItemCaseSensitive(potential_child->components, "relationships");
    const char *parent = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(relationships, "parent"));
    if(parent && strcmp(parent, removed_entity->id) == 0)
    {
      arr_push(&children, potential_child);
    }
  }

  for(size_t i = 0; i < children.length; i++)
  {
    allo_entity *child = children.data[i];
    if(mode == AlloRemovalCascade)
    {
      allo_state_remove_entity(state, child, mode);
    }
    else if(mode == AlloRemovalReparent)
    {
      cJSON_DeleteItemFromObject(child->components, "relationships");
    }
  }

  entity_destroy(state, removed_entity);
  arr_free(&children);
  return true;
}