#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include <allonet/arr.h>
#include <assert.h>
#include <math.h>

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
  destination->poses.head.grab.entity = allo_strdup(original->poses.head.grab.entity);
  destination->poses.left_hand.grab.entity = allo_strdup(original->poses.left_hand.grab.entity);
  destination->poses.right_hand.grab.entity = allo_strdup(original->poses.right_hand.grab.entity);
  destination->entity_id = allo_strdup(original->entity_id);
}

static cJSON* grab_to_cjson(allo_client_pose_grab grab)
{
  return cjson_create_object(
    "entity", cJSON_CreateString(grab.entity ? grab.entity : ""),
    "grabber_from_entity_transform", m2cjson(grab.grabber_from_entity_transform),
    NULL
  );
}

static allo_client_pose_grab grab_parse_cjson(cJSON* cjson)
{
  return (allo_client_pose_grab) {
    .entity = allo_strdup(cJSON_GetStringValue(cJSON_GetObjectItem(cjson, "entity"))),
    .grabber_from_entity_transform = cjson2m(cJSON_GetObjectItem(cjson, "grabber_from_entity_transform"))
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
  intent->entity_id = allo_strdup(cJSON_GetStringValue(cJSON_GetObjectItem(from, "entity_id")));
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

allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* from_space, allo_entity* to_space)
{
  // First, convert m to world coordinates
  allo_m4x4 worldFromFromSpace = entity_get_transform_to_world(state, from_space);
  allo_m4x4 m_in_world = allo_m4x4_concat(worldFromFromSpace, m);

  allo_m4x4 worldFromToSpace = entity_get_transform_to_world(state, to_space);
  allo_m4x4 toSpaceFromWorld = allo_m4x4_inverse(worldFromToSpace);

  return allo_m4x4_concat(toSpaceFromWorld, m_in_world);
}

void entity_set_transform(allo_entity* entity, allo_m4x4 m)
{
  for (int i = 0; i < 16; i++)
  {
    assert(isnan(m.v[i]) == false);
  }
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

allo_entity* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent)
{
  char eid[11] = { 0 };
  for (int i = 0; i < 10; i++)
  {
    eid[i] = 'a' + rand() % 25;
  }
  allo_entity* e = entity_create(eid);
  e->owner_agent_id = strdup(agent_id ? agent_id : "place");
  cJSON* children = cJSON_DetachItemFromObject(spec, "children");
  e->components = spec;

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

  printf("Creating entity %s\n", e->id);
  LIST_INSERT_HEAD(&state->entities, e, pointers);


  cJSON* child = NULL;
  cJSON_ArrayForEach(child, children) {
    allo_state_add_entity_from_spec(state, agent_id, child, eid);
  }
  return e;
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
    interaction->type = allo_strdup(type);
    interaction->sender_entity_id = allo_strdup(sender_entity_id);
    interaction->receiver_entity_id  = allo_strdup(receiver_entity_id);
    interaction->request_id = allo_strdup(request_id);
    interaction->body = allo_strdup(body);
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