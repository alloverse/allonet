#include <allonet/state/intent.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

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