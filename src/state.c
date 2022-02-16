#include <allonet/state.h>
#include <allonet/schema/reflection_reader.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include <allonet/arr.h>
#include <assert.h>
#include <math.h>
#include "alloverse_binary_schema.h"

static reflection_Schema_table_t g_alloschema;

void allo_generate_id(char *str, size_t len)
{
  for (size_t i = 0; i < len-1; i++)
  {
    str[i] = 'a' + rand() % 25;
  }
  str[len-1] = 0;
}

allo_client_intent* allo_client_intent_create()
{
  allo_client_intent* intent = calloc(1, sizeof(allo_client_intent));
  intent->poses.head.matrix = 
    intent->poses.root.matrix = 
    intent->poses.torso.matrix = 
    intent->poses.left_hand.matrix = 
    intent->poses.right_hand.matrix = allo_m4x4_identity();
  for(int i = 0; i < ALLO_HAND_SKELETON_JOINT_COUNT; i++)
  {
    intent->poses.left_hand.skeleton[i] = allo_m4x4_identity();
    intent->poses.right_hand.skeleton[i] = allo_m4x4_identity();
  }
  return intent;
}
void allo_client_intent_free(allo_client_intent* intent)
{
  free(intent->poses.left_hand.grab.entity);
  free(intent->poses.right_hand.grab.entity);
  free(intent->entity_id);
  free(intent);
}

void allo_client_intent_clone(const allo_client_intent* original, allo_client_intent* destination)
{
  free(destination->poses.left_hand.grab.entity);
  free(destination->poses.right_hand.grab.entity);
  free(destination->entity_id);
  memcpy(destination, original, sizeof(allo_client_intent));
  destination->poses.left_hand.grab.entity = allo_strdup(original->poses.left_hand.grab.entity);
  destination->poses.right_hand.grab.entity = allo_strdup(original->poses.right_hand.grab.entity);
  destination->entity_id = allo_strdup(original->entity_id);
}

static cJSON *skeleton_to_cjson(const allo_m4x4 skeleton[26])
{
  cJSON *list = cJSON_CreateArray();
  // optimization: 3 is thumb root. if it's exactly identity, it's very likely the whole
  // list of bones is identity, so just don't send it.
  // we could also have an explicit "send skeleton" bool but that propagates a lot of
  // layers so I'm gonna try this opt first...
  if(allo_m4x4_is_identity(skeleton[3])) {
    return list;
  }
  for(int i = 0; i < ALLO_HAND_SKELETON_JOINT_COUNT; i++)
  {
    cJSON_AddItemToArray(list, m2cjson(skeleton[i]));
  }
  return list;
}

static void cjson_to_skeleton(allo_m4x4 skeleton[26], cJSON *list)
{
  if(list == NULL || list->child == NULL)
  {
    return;
  }
  
  cJSON *node = list->child;
  int i = 0;
  while(node && i < ALLO_HAND_SKELETON_JOINT_COUNT)
  {
    skeleton[i] = cjson2m(node);
    node = node->next;
  }
}

static cJSON* grab_to_cjson(allo_client_pose_grab grab)
{
  if(grab.entity == NULL)
  {
    return cJSON_CreateObject();
  }
  
  return cjson_create_object(
    "entity", cJSON_CreateString(grab.entity ? grab.entity : ""),
    "grabber_from_entity_transform", m2cjson(grab.grabber_from_entity_transform),
    NULL
  );
}

static allo_client_pose_grab grab_parse_cjson(cJSON* cjson)
{
  return (allo_client_pose_grab) {
    .entity = allo_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cjson, "entity"))),
    .grabber_from_entity_transform = cjson2m(cJSON_GetObjectItemCaseSensitive(cjson, "grabber_from_entity_transform"))
  };
}

cJSON* allo_client_intent_to_cjson(const allo_client_intent* intent)
{
  return cjson_create_object(
    "wants_stick_movement", cJSON_CreateBool(intent->wants_stick_movement),
    "zmovement", cJSON_CreateNumber(intent->zmovement),
    "xmovement", cJSON_CreateNumber(intent->xmovement),
    "yaw", cJSON_CreateNumber(intent->yaw),
    "pitch", cJSON_CreateNumber(intent->pitch),
    "poses", cjson_create_object(
      "root", cjson_create_object(
        "matrix", m2cjson(intent->poses.root.matrix),
        NULL
      ),
      "head", cjson_create_object(
        "matrix", m2cjson(intent->poses.head.matrix),
        NULL
      ),
      "torso", cjson_create_object(
        "matrix", m2cjson(intent->poses.torso.matrix),
        NULL
      ),
      "hand/left", cjson_create_object(
        "matrix", m2cjson(intent->poses.left_hand.matrix),
        "skeleton", skeleton_to_cjson(intent->poses.left_hand.skeleton),
        "grab", grab_to_cjson(intent->poses.left_hand.grab),
        NULL
      ),
      "hand/right", cjson_create_object(
        "matrix", m2cjson(intent->poses.right_hand.matrix),
        "skeleton", skeleton_to_cjson(intent->poses.right_hand.skeleton),
        "grab", grab_to_cjson(intent->poses.right_hand.grab),
        NULL
      ),
      NULL
    ),
    "ack_state_rev", cJSON_CreateNumber(intent->ack_state_rev),
    NULL
  );
}

allo_client_intent *allo_client_intent_parse_cjson(const cJSON* from)
{
  allo_client_intent* intent = allo_client_intent_create();
  cJSON* poses = cJSON_GetObjectItemCaseSensitive(from, "poses");
  intent->entity_id = allo_strdup(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(from, "entity_id")));
  cJSON *wants = cJSON_GetObjectItemCaseSensitive(from, "wants_stick_movement");
  intent->wants_stick_movement = wants ? wants->valueint : true;
  intent->zmovement = cJSON_GetObjectItemCaseSensitive(from, "zmovement")->valuedouble;
  intent->xmovement = cJSON_GetObjectItemCaseSensitive(from, "xmovement")->valuedouble;
  intent->yaw = cJSON_GetObjectItemCaseSensitive(from, "yaw")->valuedouble;
  intent->pitch = cJSON_GetObjectItemCaseSensitive(from, "pitch")->valuedouble;
  cJSON *hand_left = cJSON_GetObjectItemCaseSensitive(poses, "hand/left");
  cJSON *hand_right = cJSON_GetObjectItemCaseSensitive(poses, "hand/right");
  intent->poses = (allo_client_poses){
    .root = (allo_client_plain_pose){
      .matrix = cjson2m(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(poses, "root"), "matrix")),
    },
    .head = (allo_client_plain_pose){
      .matrix = cjson2m(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(poses, "head"), "matrix")),
    },
    .torso = (allo_client_plain_pose){
      .matrix = cjson2m(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(poses, "torso"), "matrix")),
    },
    .left_hand = (allo_client_hand_pose){
      .matrix = cjson2m(cJSON_GetObjectItemCaseSensitive(hand_left, "matrix")),
      .grab = grab_parse_cjson(cJSON_GetObjectItemCaseSensitive(hand_left, "grab"))
    },
    .right_hand = (allo_client_hand_pose){
      .matrix = cjson2m(cJSON_GetObjectItemCaseSensitive(hand_right, "matrix")),
      .grab = grab_parse_cjson(cJSON_GetObjectItemCaseSensitive(hand_right, "grab"))
    },
  };
  cjson_to_skeleton(intent->poses.left_hand.skeleton, cJSON_GetObjectItemCaseSensitive(hand_left, "skeleton"));
  cjson_to_skeleton(intent->poses.right_hand.skeleton, cJSON_GetObjectItemCaseSensitive(hand_right, "skeleton"));

  intent->ack_state_rev = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(from, "ack_state_rev"));
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

allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* old, allo_entity* new)
{
  allo_m4x4 worldFromOld = entity_get_transform_to_world(state, old);
  allo_m4x4 worldFromNew = entity_get_transform_to_world(state, new);
  allo_m4x4 newFromWorld = allo_m4x4_inverse(worldFromNew);
  allo_m4x4 newFromOld = allo_m4x4_concat(newFromWorld, worldFromOld);

  return allo_m4x4_concat(newFromOld, m);
}

void allo_state_init(allo_state *state)
{
  state->revision = 1;
  LIST_INIT(&state->entities);
}

void allo_state_destroy(allo_state *state)
{
  allo_entity *entity = state->entities.lh_first;
  while(entity)
  {
    allo_entity *to_delete = entity;
    entity = entity->pointers.le_next;
    entity_destroy(to_delete);
  }
}

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

extern void allo_state_diff_init(allo_state_diff *diff)
{
  arr_init(&diff->new_entities); arr_reserve(&diff->new_entities, 64);
  arr_init(&diff->deleted_entities); arr_reserve(&diff->deleted_entities, 64);
  arr_init(&diff->new_components); arr_reserve(&diff->new_components, 64);
  arr_init(&diff->updated_components); arr_reserve(&diff->updated_components, 64);
  arr_init(&diff->deleted_components); arr_reserve(&diff->deleted_components, 64);
}
extern void allo_state_diff_free(allo_state_diff *diff)
{
  arr_free(&diff->new_entities);
  arr_free(&diff->deleted_entities);
  arr_free(&diff->new_components);
  arr_free(&diff->updated_components);
  arr_free(&diff->deleted_components);
}
extern void allo_state_diff_dump(allo_state_diff *diff)
{
  printf("=============== Statediff ================\n");
  for(size_t i = 0; i < diff->new_entities.length; i++)
  {
    printf("New entity: %s\n", diff->new_entities.data[i]);
  }
  for(size_t i = 0; i < diff->deleted_entities.length; i++)
  {
    printf("Deleted entity: %s\n", diff->deleted_entities.data[i]);
  }
  for(size_t i = 0; i < diff->new_components.length; i++)
  {
    printf("New component: %s.%s\n", diff->new_components.data[i].eid, diff->new_components.data[i].name);
  }
  for(size_t i = 0; i < diff->updated_components.length; i++)
  {
    printf("Updated component: %s.%s\n", diff->updated_components.data[i].eid, diff->updated_components.data[i].name);
  }
  for(size_t i = 0; i < diff->deleted_components.length; i++)
  {
    printf("Deleted component: %s.%s\n", diff->deleted_components.data[i].eid, diff->deleted_components.data[i].name);
  }
}
extern void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->new_components, ref);
}
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->updated_components, ref);
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



allo_entity* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent)
{
  char generated_eid[11] = { 0 };
  allo_generate_id(generated_eid, 11);
  const char *eid = generated_eid;

  allo_entity* e = entity_create(eid);
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

  entity_destroy(removed_entity);
  arr_free(&children);
  return true;
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



void allo_state_to_flat(allo_state *state, flatcc_builder_t *B)
{
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

    _allo_media_initialize();

    g_alloschema = reflection_Schema_as_root(alloverse_schema_bytes);

    return true;
}

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
    allo_interaction *interaction = (allo_interaction*)malloc(sizeof(allo_interaction));
    interaction->type = allo_strdup(type);
    interaction->sender_entity_id = allo_strdup(sender_entity_id);
    interaction->receiver_entity_id = allo_strdup(receiver_entity_id);
    interaction->request_id = allo_strdup(request_id);
    interaction->body = allo_strdup(body);
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

allo_interaction *allo_interaction_clone(const allo_interaction *interaction)
{
  return allo_interaction_create(interaction->type, interaction->sender_entity_id, interaction->receiver_entity_id, interaction->request_id, interaction->body);
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
  allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
  free((void*)bodystr);
  return interaction;
}

