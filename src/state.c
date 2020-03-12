#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"


allo_entity *entity_create(const char *id)
{
    allo_entity *entity = (allo_entity *)calloc(1, sizeof(allo_entity));
    entity->id = strdup(id);
    return entity;
}
void entity_destroy(allo_entity *entity)
{
    cJSON_Delete(entity->components);
    free(entity->id);
    free(entity);
}

extern allo_m4x4 entity_get_transform(allo_entity* entity)
{
  cJSON* transform = cJSON_GetObjectItem(entity->components, "transform");
  cJSON* matrix = cJSON_GetObjectItem(transform, "matrix");
  if (!transform || !matrix || cJSON_GetArraySize(matrix) != 16)
    return allo_m4x4_identity();

  allo_m4x4 m;
  for (int i = 0; i < 16; i++) {
    m.v[i] = cJSON_GetArrayItem(matrix, i)->valuedouble;
  }
  return m;
}

void entity_set_transform(allo_entity* entity, allo_m4x4 m)
{
  cJSON* transform = cJSON_GetObjectItem(entity->components, "transform");
  cJSON* matrix = cJSON_GetObjectItem(transform, "matrix");
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

allo_entity* state_get_entity(allo_state* state, const char* entity_id)
{
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

// move to allonet.c
#include <enet/enet.h>
extern bool allo_initialize(bool redirect_stdout)
{
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
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        return false;
    }
    atexit (enet_deinitialize);

    return true;
}

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
        allo_interaction *interaction = (allo_interaction*)malloc(sizeof(allo_interaction));
        interaction->type = strdup(type);
        interaction->sender_entity_id = strdup(sender_entity_id);
        interaction->receiver_entity_id  = strdup(receiver_entity_id);
        interaction->request_id = strdup(request_id);
        interaction->body = strdup(body);
        return interaction;
}
void allo_interaction_free(allo_interaction *interaction)
{
    free((void*)interaction->type);
    free((void*)interaction->sender_entity_id);
    free((void*)interaction->receiver_entity_id);
    free((void*)interaction->request_id);
    free((void*)interaction->body);
    free(interaction);
}