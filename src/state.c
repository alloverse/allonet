#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"

void allo_client_intent_initialize(allo_client_intent *intent)
{
    memset(intent, 0, sizeof(*intent));
    intent->poses.head.matrix = intent->poses.left_hand.matrix = intent->poses.right_hand.matrix = allo_m4x4_identity();
}

cJSON* allo_client_intent_to_cjson(const allo_client_intent* intent)
{
  return cjson_create_object(
    "zmovement", cJSON_CreateNumber(intent->zmovement),
    "xmovement", cJSON_CreateNumber(intent->xmovement),
    "yaw", cJSON_CreateNumber(intent->yaw),
    "pitch", cJSON_CreateNumber(intent->pitch),
    "poses", cjson_create_object(
      "head", cjson_create_object(
        "matrix", m2cjson(intent->poses.head.matrix),
        NULL
      ),
      "hand/left", cjson_create_object(
        "matrix", m2cjson(intent->poses.left_hand.matrix),
        NULL
      ),
      "hand/right", cjson_create_object(
        "matrix", m2cjson(intent->poses.right_hand.matrix),
        NULL
      ),
      NULL
    ),
    NULL
  );
}

allo_client_intent allo_client_intent_parse_cjson(const cJSON* from)
{
  cJSON* poses = cJSON_GetObjectItem(from, "poses");
  return (allo_client_intent) {
    .zmovement = cJSON_GetObjectItem(from, "zmovement")->valuedouble,
      .xmovement = cJSON_GetObjectItem(from, "xmovement")->valuedouble,
      .yaw = cJSON_GetObjectItem(from, "yaw")->valuedouble,
      .pitch = cJSON_GetObjectItem(from, "pitch")->valuedouble,
      .poses = (allo_client_poses){
        .head = (allo_client_pose){.matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(from, "head"), "matrix"))},
        .left_hand = (allo_client_pose){.matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(from, "left_hand"), "matrix"))},
        .right_hand = (allo_client_pose){.matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(from, "right_hand"), "matrix"))},
      }
  };
}


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
    free(entity->owner_agent_id);
    free(entity);
}

extern allo_m4x4 entity_get_transform(allo_entity* entity)
{
  if(!entity)
    return allo_m4x4_identity();
  cJSON* transform = cJSON_GetObjectItem(entity->components, "transform");
  cJSON* matrix = cJSON_GetObjectItem(transform, "matrix");
  if (!transform || !matrix || cJSON_GetArraySize(matrix) != 16)
    return allo_m4x4_identity();

  return cjson2m(matrix);
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

// move to allonet.c
#include <enet/enet.h>
static bool _has_initialized = false;
extern bool allo_initialize(bool redirect_stdout)
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

cJSON* allo_interaction_to_cjson(const allo_interaction* interaction)
{
  return cjson_create_list(
    cJSON_CreateString("interaction"),
    cJSON_CreateString(interaction->type),
    cJSON_CreateString(interaction->sender_entity_id),
    cJSON_CreateString(interaction->receiver_entity_id),
    cJSON_CreateString(interaction->request_id),
    cJSON_Parse(interaction->body),
    NULL
  );
}
allo_interaction *allo_interaction_parse_cjson(const cJSON* inter_json)
{
  const char* type = cJSON_GetArrayItem(inter_json, 1)->valuestring;
  const char* from = cJSON_GetArrayItem(inter_json, 2)->valuestring;
  const char* to = cJSON_GetArrayItem(inter_json, 3)->valuestring;
  const char* request_id = cJSON_GetArrayItem(inter_json, 4)->valuestring;
  cJSON* body = cJSON_GetArrayItem(inter_json, 5);
  const char* bodystr = cJSON_Print(body);
  return allo_interaction_create(type, from, to, request_id, bodystr);
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