#include <allonet/state.h>
#include <allonet/schema/reflection_reader.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../util.h"
#include <allonet/arr.h>
#include <assert.h>
#include <math.h>
#include "../alloverse_binary_schema.h"

static reflection_Schema_table_t g_alloschema;
static reflection_Object_vec_t SchemaTables;

cJSON *allo_state_to_json(allo_state *state, bool include_agent_id)
{
  cJSON* entities_rep = cJSON_CreateObject();
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* entity_rep = cjson_create_object(
      "id", cJSON_CreateString(entity->id),
      NULL
    );
    if(include_agent_id && entity->owner_agent_id)
    {
      cJSON_AddItemToObject(entity_rep, "agent_id", cJSON_CreateString(entity->owner_agent_id));
    }
    cJSON_AddItemToObject(entity_rep, "components", cJSON_Duplicate(entity->components, 1));
    cJSON_AddItemToObject(entities_rep, entity->id, entity_rep);
  }
  cJSON* map = cjson_create_object(
    "entities", entities_rep,
    "revision", cJSON_CreateNumber(state->revision),
    NULL
  );
  return map;
}

allo_state *allo_state_from_json(cJSON *json)
{
  allo_state *state = calloc(1, sizeof(allo_state));
  allo_state_init(state);

  state->revision = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "revision"));
  cJSON *entitiesrep = cJSON_GetObjectItemCaseSensitive(json, "entities");
  if(state->revision == 0 || entitiesrep == NULL)
  {
    free(state);
    return NULL;
  }

  cJSON* entrep = entitiesrep->child;
  while (entrep)
  {
    cJSON* next = entrep->next;
    cJSON* spec = cJSON_DetachItemFromObjectCaseSensitive(entrep, "components");
    const char *eid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(entrep, "id")); (void)eid;
    const char *agent_id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(entrep, "agent_id"));
    allo_entity *ent = allo_state_add_entity_from_spec(state, agent_id, spec, NULL);
    ent->id = strdup(eid);
    entrep = next;
  }


  return state;
}

void allo_state_to_flat(allo_state *state, flatcc_builder_t *B)
{
  reflection_Object_table_t Components = reflection_Object_vec_find_by_name(SchemaTables, "Components");

  Alloverse_State_start_as_root(B);
  Alloverse_State_revision_add(B, state->revision);

  Alloverse_State_entities_start(B);
  
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    Alloverse_State_entities_push_start(B);
    Alloverse_Entity_id_create_str(B, entity->id);

    Alloverse_Entity_components_start(B);

    cJSON* comp = entity->components->child;
    while (comp)
    {
      // todo: use the bfbs file to parse through into FlatBuffers
      comp = comp->next;
    }

    Alloverse_Entity_components_end(B);

    Alloverse_State_entities_push_end(B);
  }
  Alloverse_State2_entities_end(B);

  Alloverse_State_end_as_root(B);
}

// move to allonet.c
#include <enet/enet.h>
static bool _has_initialized = false;
bool allo_initialize(bool redirect_stdout)
{
    if (_has_initialized) return true;
    _has_initialized = true;

    setvbuf(stdout, NULL, _IONBF, 0);
    if(redirect_stdout) {
        printf("Moving stdout...\n");
        fflush(stdout);
        freopen("/tmp/debug.txt", "a", stdout);
        setvbuf(stdout, NULL, _IONBF, 0);
        printf("Stdout moved\n");
        fflush(stdout);
    }
    if (enet_initialize () != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return false;
    }
    atexit (enet_deinitialize);

    _allo_media_initialize();

    g_alloschema = reflection_Schema_as_root(alloverse_schema_bytes);
    assert(g_alloschema);
    if(!g_alloschema)
    {
        fprintf(stderr, "Allonet was unable to parse its flatbuffer schema.\n");
        return false;
    }
    SchemaTables = reflection_Schema_objects(g_alloschema);

    return true;
}

