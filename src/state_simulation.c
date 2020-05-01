#include "allonet/state.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void move_avatar(allo_entity* avatar, allo_entity* head, allo_client_intent intent, double dt);
static void move_pose(allo_state* state, allo_entity* ent, allo_client_intent intent, double dt);

static allo_entity *get_child_with_pose(allo_state *state, allo_entity *avatar, const char *pose_name)
{
  if (!state || !avatar || !pose_name || strlen(pose_name) == 0)
    return NULL;
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON *relationships = cJSON_GetObjectItem(entity->components, "relationships");
    cJSON *parent = cJSON_GetObjectItem(relationships, "parent");
    cJSON *intent = cJSON_GetObjectItem(entity->components, "intent");
    cJSON *actuate_pose = cJSON_GetObjectItem(intent, "actuate_pose");
    
    if (parent && actuate_pose && strcmp(parent->valuestring, avatar->id) == 0 && strcmp(actuate_pose->valuestring, pose_name) == 0)
    {
      return entity;
    }
  }
  return NULL;
}

extern void allo_simulate(allo_state* state, double dt, allo_client_intent* intents, int intent_count)
{
  for (int i = 0; i < intent_count; i++)
  {
    allo_client_intent intent = intents[i];
    allo_entity* avatar = state_get_entity(state, intent.entity_id);
    if (intent.entity_id == NULL || avatar == NULL)
      return;
    allo_entity* head = get_child_with_pose(state, avatar, "head");
    move_avatar(avatar, head, intent, dt);
    move_pose(state, avatar, intent, dt);
  }
}

// removes pitch from transform so it becomes a suitable walking direction
static allo_m4x4 constrain_head_pitch(allo_m4x4 transform)
{
  allo_vector origin = {0, 0, 0};
  allo_vector forward = {0, 0, 1};
  allo_vector position = allo_m4x4_transform(transform, origin, true);
  allo_vector moved = allo_m4x4_transform(transform, forward, true);
  allo_vector direction = allo_vector_subtract(moved, position);  
  allo_vector ground_direction = allo_vector_subtract(direction, (allo_vector){0, direction.y, 0});
  // negate any vertical angle
  double pitch = allo_vector_angle(ground_direction, direction);
  if (direction.y < 0) pitch *= -1;
  allo_m4x4 constrainer = allo_m4x4_rotate(pitch, (allo_vector){1, 0, 0});
  return allo_m4x4_concat(constrainer, transform);
}

static allo_m4x4 create_movement(allo_m4x4 head_transform, double yaw, double xmovement, double zmovement)
{
  allo_m4x4 inverse_head = allo_m4x4_inverse(head_transform);
  // intent movement is always relative to the facing direction of the user, controlled
  // by the head transform and yaw intent.
  // Begin by creating a matrix representing that yaw and one for user-relative translation...
  allo_m4x4 rotation = allo_m4x4_rotate(yaw, (allo_vector) { 0, -1, 0 });
  allo_m4x4 translation = allo_m4x4_translate((allo_vector) { xmovement, 0, zmovement });
  // then combine head transform, rotation and translation to create a movement matrix,
  return allo_m4x4_concat(inverse_head, allo_m4x4_concat(translation, allo_m4x4_concat(head_transform, rotation)));
}

static void move_avatar(allo_entity* avatar, allo_entity* head, allo_client_intent intent, double dt)
{
  if(intent.xmovement == 0 && intent.zmovement == 0 && intent.yaw == 0)
    return;
  
  double speed = 2.0; // meters per second
  double distance = speed * dt;

  allo_m4x4 old_transform = entity_get_transform(avatar);
  allo_m4x4 raw_head_transform = entity_get_transform(head);
  allo_m4x4 head_transform = constrain_head_pitch(raw_head_transform);
  allo_m4x4 movement = create_movement(head_transform, intent.yaw, intent.xmovement * distance, intent.zmovement * distance);

  // which can then be concat'd into the old transform.
  allo_vector old_position = allo_m4x4_transform(old_transform, (allo_vector){ 0, 0, 0 }, true);
  allo_m4x4 old_positional_transform = allo_m4x4_translate(old_position);

  // now we gotta compensate: rotating the avatar will MOVE the head if it's not in origin, so we'll have to move
  // the avatar so the head stays in place.
  allo_m4x4 head_worldcoords = allo_m4x4_concat(raw_head_transform, old_transform);

  allo_m4x4 just_rotation = create_movement(head_transform, intent.yaw, 0, 0);
  allo_m4x4 new_transform_r = allo_m4x4_concat(just_rotation, old_positional_transform);
  allo_m4x4 new_head_worldcoords = allo_m4x4_concat(raw_head_transform, new_transform_r);
  allo_vector a = allo_m4x4_transform(head_worldcoords, (allo_vector){ 0, 0, 0 }, true);
  allo_vector b = allo_m4x4_transform(new_head_worldcoords, (allo_vector){ 0, 0, 0 }, true);
  allo_vector head_movement = allo_vector_subtract(a, b);
  allo_m4x4 keep_head_centered = allo_m4x4_translate(head_movement);

  // Now calculate the new transform for the root entity
  allo_m4x4 new_transform = allo_m4x4_concat(movement, old_positional_transform);
  allo_m4x4 new_transform2 = allo_m4x4_concat(new_transform, keep_head_centered);

  entity_set_transform(avatar, new_transform2);
}

static void move_pose(allo_state* state, allo_entity* avatar, allo_client_intent intent, double dt)
{
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* rels = cJSON_GetObjectItem(entity->components, "relationships");
    cJSON* parent = cJSON_GetObjectItem(rels, "parent");
    cJSON* intents = cJSON_GetObjectItem(entity->components, "intent");
    cJSON* actuate_pose = cJSON_GetObjectItem(intents, "actuate_pose");
    if (!parent || !actuate_pose || strcmp(avatar->id, parent->valuestring) != 0)
      continue;
    const char* posename = actuate_pose->valuestring;

    allo_m4x4 new_transform;
    if (strcmp(posename, "hand/left") == 0) new_transform = intent.poses.left_hand.matrix;
    else if (strcmp(posename, "hand/right") == 0) new_transform = intent.poses.right_hand.matrix;
    else if (strcmp(posename, "head") == 0) new_transform = intent.poses.head.matrix;
    else continue;

    entity_set_transform(entity, new_transform);
  }

}

