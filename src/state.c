#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include <allonet/arr.h>

allo_client_intent* allo_client_intent_create()
{
  allo_client_intent* intent = calloc(1, sizeof(allo_client_intent));
  intent->poses.head.matrix = intent->poses.left_hand.matrix = intent->poses.right_hand.matrix = allo_m4x4_identity();
  return intent;
}
void allo_client_intent_free(allo_client_intent* intent)
{
  free(intent->poses.head.grab.entity);
  free(intent->poses.left_hand.grab.entity);
  free(intent->poses.right_hand.grab.entity);
  free(intent->entity_id);
  free(intent);
}

void allo_client_intent_clone(const allo_client_intent* original, allo_client_intent* destination)
{
  free(destination->poses.head.grab.entity);
  free(destination->poses.left_hand.grab.entity);
  free(destination->poses.right_hand.grab.entity);
  free(destination->entity_id);
  memcpy(destination, original, sizeof(allo_client_intent));
  destination->poses.head.grab.entity = strdup(original->poses.head.grab.entity);
  destination->poses.left_hand.grab.entity = strdup(original->poses.left_hand.grab.entity);
  destination->poses.right_hand.grab.entity = strdup(original->poses.right_hand.grab.entity);
  destination->entity_id = strdup(original->entity_id);
}

static cJSON* grab_to_cjson(allo_client_pose_grab grab)
{
  return cjson_create_object(
    "entity", cJSON_CreateString(grab.entity ? grab.entity : ""),
    "held_at", vec2cjson(grab.held_at),
    NULL
  );
}

static allo_client_pose_grab grab_parse_cjson(cJSON* cjson)
{
  return (allo_client_pose_grab) {
    .entity = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(cjson, "entity"))),
    .held_at = cjson2vec(cJSON_GetObjectItem(cjson, "held_at"))
  };
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
        "grab", grab_to_cjson(intent->poses.head.grab),
        NULL
      ),
      "hand/left", cjson_create_object(
        "matrix", m2cjson(intent->poses.left_hand.matrix),
        "grab", grab_to_cjson(intent->poses.left_hand.grab),
        NULL
      ),
      "hand/right", cjson_create_object(
        "matrix", m2cjson(intent->poses.right_hand.matrix),
        "grab", grab_to_cjson(intent->poses.right_hand.grab),
        NULL
      ),
      NULL
    ),
    NULL
  );
}

allo_client_intent *allo_client_intent_parse_cjson(const cJSON* from)
{
  allo_client_intent* intent = allo_client_intent_create();
  cJSON* poses = cJSON_GetObjectItem(from, "poses");
  intent->entity_id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(from, "entity_id")));
  intent->zmovement = cJSON_GetObjectItem(from, "zmovement")->valuedouble;
  intent->xmovement = cJSON_GetObjectItem(from, "xmovement")->valuedouble;
  intent->yaw = cJSON_GetObjectItem(from, "yaw")->valuedouble;
  intent->pitch = cJSON_GetObjectItem(from, "pitch")->valuedouble;
  intent->poses = (allo_client_poses){
    .head = (allo_client_pose){
      .matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "head"), "matrix")),
      .grab = grab_parse_cjson(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "head"), "grab"))
    },
    .left_hand = (allo_client_pose){
      .matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "hand/left"), "matrix")),
      .grab = grab_parse_cjson(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "hand/left"), "grab"))
    },
    .right_hand = (allo_client_pose){
      .matrix = cjson2m(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "hand/right"), "matrix")),
      .grab = grab_parse_cjson(cJSON_GetObjectItem(cJSON_GetObjectItem(poses, "hand/right"), "grab"))
    },
  };
  return intent;
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

extern allo_entity* entity_get_parent(allo_state* state, allo_entity* entity)
{
  cJSON* relationships = cJSON_GetObjectItem(entity->components, "relationships");
  cJSON* parentIdJ = cJSON_GetObjectItem(relationships, "parent");
  if (!parentIdJ) return NULL;
  return state_get_entity(state, cJSON_GetStringValue(parentIdJ));
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

allo_m4x4 entity_get_transform_in_coordinate_space(allo_state *state, allo_entity* entity, allo_entity* space)
{
  allo_m4x4 m = entity_get_transform(entity);
  // go up to root, multiplying with each parent until we reach world space coordinates
  while (entity != space && entity) {
    entity = entity_get_parent(state, entity);
    m = allo_m4x4_concat(entity_get_transform(entity), m);
  }

  // go down to space until we reach the local coordinate space of `space`
  // first, compile list of ancestors so we can traverse it in reverse
  arr_t(allo_m4x4) spaces = { 0 };
  while (space) {
    arr_push(&spaces, entity_get_transform(space));
    space = entity_get_parent(state, space);
  }
  // then concatenate down until we're in the local space of `space`.
  for (int i = spaces.length; i-- > 0;)
  {
    m = allo_m4x4_concat(m, spaces.data[i]);
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