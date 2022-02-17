#include <allonet/state/state_write.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

using namespace Alloverse;
using namespace std;

allo_mutable_state::allo_mutable_state()
{
    revision = 1;
}


Alloverse::EntityT *entity_create(allo_mutable_state *state, const char *id)
{
    auto ent = unique_ptr<EntityT>(new EntityT());
    ent->id = id;
    next(state)->entities.push_back(ent);
    return ent.get();
}

void entity_destroy(allo_mutable_state *state, Alloverse::EntityT *entity)
{

}



Alloverse::EntityT* allo_state_add_entity_from_spec(allo_mutable_state* state, const char* agent_id, cJSON* spec, const char* parent)
{
  char generated_eid[11] = { 0 };
  allo_generate_id(generated_eid, 11);
  const char *eid = generated_eid;

  Alloverse::EntityT* e = entity_create(state, eid);
  e->owner_agent_id = agent_id ? agent_id : "place";
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

bool allo_state_remove_entity_id(allo_mutable_state *state, const char *eid, allo_removal_mode mode)
{
  allo_entity* removed_entity = state_get_entity(state, eid);
  if(!removed_entity)
  {
    return false;
  }
  return allo_state_remove_entity(state, removed_entity, mode);
}
bool allo_state_remove_entity(allo_mutable_state *state, allo_entity *removed_entity, allo_removal_mode mode)
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
