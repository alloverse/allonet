#include "allonet/state.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void move_avatar(allo_entity* avatar, allo_entity* head, const allo_client_intent *intent, double dt);
static void move_pose(allo_state* state, allo_entity* ent, const allo_client_intent *intent, double dt);
static void handle_grabs(allo_state *state, allo_entity *avatar, const allo_client_intent *intent, double dt);

static allo_entity* get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name);

extern void allo_simulate(allo_state* state, double dt, const allo_client_intent** intents, int intent_count)
{
  for (int i = 0; i < intent_count; i++)
  {
    const allo_client_intent *intent = intents[i];
    allo_entity* avatar = state_get_entity(state, intent->entity_id);
    if (intent->entity_id == NULL || avatar == NULL)
      return;
    allo_entity* head = get_child_with_pose(state, avatar, "head");
    move_avatar(avatar, head, intent, dt);
    move_pose(state, avatar, intent, dt);
    handle_grabs(state, avatar, intent, dt);
  }
}


static allo_entity* get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name)
{
  if (!state || !avatar || !pose_name || strlen(pose_name) == 0)
    return NULL;
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* relationships = cJSON_GetObjectItem(entity->components, "relationships");
    cJSON* parent = cJSON_GetObjectItem(relationships, "parent");
    cJSON* intent = cJSON_GetObjectItem(entity->components, "intent");
    cJSON* actuate_pose = cJSON_GetObjectItem(intent, "actuate_pose");

    if (parent && actuate_pose && strcmp(parent->valuestring, avatar->id) == 0 && strcmp(actuate_pose->valuestring, pose_name) == 0)
    {
      return entity;
    }
  }
  return NULL;
}

// removes pitch from transform so it becomes a suitable walking direction
static allo_m4x4 constrain_head_pitch(allo_m4x4 transform)
{
  allo_vector origin = {{0, 0, 0}};
  allo_vector forward = {{0, 0, 1}};
  allo_vector position = allo_m4x4_transform(transform, origin, true);
  allo_vector moved = allo_m4x4_transform(transform, forward, true);
  allo_vector direction = allo_vector_subtract(moved, position);  
  allo_vector ground_direction = allo_vector_subtract(direction, (allo_vector){{0, direction.y, 0}});
  // negate any vertical angle
  double pitch = allo_vector_angle(ground_direction, direction);
  if (direction.y < 0) pitch *= -1;
  allo_m4x4 constrainer = allo_m4x4_rotate(pitch, (allo_vector){{1, 0, 0}});
  return allo_m4x4_concat(transform, constrainer);
}

static allo_m4x4 create_movement(allo_m4x4 head_transform, double yaw, double xmovement, double zmovement)
{
  allo_m4x4 inverse_head = allo_m4x4_inverse(head_transform);
  // intent movement is always relative to the facing direction of the user, controlled
  // by the head transform and yaw intent.
  // Begin by creating a matrix representing that yaw and one for user-relative translation...
  allo_m4x4 rotation = allo_m4x4_rotate(yaw, (allo_vector) {{ 0, -1, 0 }});
  allo_m4x4 translation = allo_m4x4_translate((allo_vector) {{ xmovement, 0, zmovement }});
  // then combine head transform, rotation and translation to create a movement matrix,
  return allo_m4x4_concat(allo_m4x4_concat(allo_m4x4_concat(rotation, head_transform), translation), inverse_head);
}

static void move_avatar(allo_entity* avatar, allo_entity* head, const allo_client_intent *intent, double dt)
{
  if(intent->xmovement == 0 && intent->zmovement == 0 && intent->yaw == 0)
    return;
  
  double speed = 2.0; // meters per second
  double distance = speed * dt;

  allo_m4x4 old_transform = entity_get_transform(avatar);
  allo_m4x4 raw_head_transform = entity_get_transform(head);
  allo_m4x4 head_transform = constrain_head_pitch(raw_head_transform);
  allo_m4x4 movement = create_movement(head_transform, intent->yaw, intent->xmovement * distance, intent->zmovement * distance);

  // which can then be concat'd into the old transform.
  allo_vector old_position = allo_m4x4_transform(old_transform, (allo_vector){{ 0, 0, 0 }}, true);
  allo_m4x4 old_positional_transform = allo_m4x4_translate(old_position);

  // now we gotta compensate: rotating the avatar will MOVE the head if it's not in origin, so we'll have to move
  // the avatar so the head stays in place.
  allo_m4x4 head_worldcoords = allo_m4x4_concat(old_transform, raw_head_transform);

  allo_m4x4 just_rotation = create_movement(head_transform, intent->yaw, 0, 0);
  allo_m4x4 new_transform_r = allo_m4x4_concat(old_positional_transform, just_rotation);
  allo_m4x4 new_head_worldcoords = allo_m4x4_concat(new_transform_r, raw_head_transform);
  allo_vector a = allo_m4x4_transform(head_worldcoords, (allo_vector){{ 0, 0, 0 }}, true);
  allo_vector b = allo_m4x4_transform(new_head_worldcoords, (allo_vector){{ 0, 0, 0 }}, true);
  allo_vector head_movement = allo_vector_subtract(a, b);
  allo_m4x4 keep_head_centered = allo_m4x4_translate(head_movement);

  // Now calculate the new transform for the root entity
  allo_m4x4 new_transform = allo_m4x4_concat(old_positional_transform, movement);
  allo_m4x4 new_transform2 = allo_m4x4_concat(keep_head_centered, new_transform);

  entity_set_transform(avatar, new_transform2);
}

static void move_pose(allo_state* state, allo_entity* avatar, const allo_client_intent *intent, double dt)
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
    if (strcmp(posename, "hand/left") == 0) new_transform = intent->poses.left_hand.matrix;
    else if (strcmp(posename, "hand/right") == 0) new_transform = intent->poses.right_hand.matrix;
    else if (strcmp(posename, "head") == 0) new_transform = intent->poses.head.matrix;
    else continue;

    entity_set_transform(entity, new_transform);
  }
}

static allo_m4x4 _constrain(allo_m4x4 orig, allo_m4x4 update, allo_vector tconstraint, allo_vector rconstraint);

static void handle_grabs(allo_state *const state, allo_entity *const avatar, const allo_client_intent *const intent, double dt)
{
  dt = 0.01;
  const allo_client_pose_grab* grabs[] = { &intent->poses.left_hand.grab, &intent->poses.right_hand.grab };
  allo_entity* grabbers[] = { get_child_with_pose(state, avatar, "hand/left"), get_child_with_pose(state, avatar, "hand/right") };
  for (int i = 0; i < 2; i++)
  {
    const allo_client_pose_grab* grab = grabs[i];
    allo_entity *const grabber = grabbers[i];
    allo_entity *const grabbed = state_get_entity(state, grab->entity);
    allo_entity * actuated = grabbed;
    cJSON *const grabbable = cJSON_GetObjectItem(grabbed ? grabbed->components : NULL, "grabbable");
    const char* actuate_on = cJSON_GetStringValue(cJSON_GetObjectItem(grabbable, "actuate_on"));
    if (actuate_on && strlen(actuate_on) > 0) {
      if (strcmp(actuate_on, "$parent") == 0) {
        actuated = entity_get_parent(state, grabbed);
      } else {
        actuated = state_get_entity(state, actuate_on);
      }
    }
    if (!grab || !grabber || !grabbed || !actuated || !grabbable) { continue; }

    cJSON* const trcj = cJSON_GetObjectItem(grabbable, "translation_constraint");
    allo_vector translation_constraint = {{ 1,1,1 }};
    if (cJSON_GetArraySize(trcj) == 3)
      translation_constraint = (allo_vector){{ cJSON_GetArrayItem(trcj, 0)->valuedouble, cJSON_GetArrayItem(trcj, 1)->valuedouble , cJSON_GetArrayItem(trcj, 2)->valuedouble }};
    cJSON* const rcj = cJSON_GetObjectItem(grabbable, "rotation_constraint");
    allo_vector rotation_constraint = {{ 1, 1, 1 }};
    if(cJSON_GetArraySize(rcj) == 3)
      rotation_constraint = (allo_vector) {{cJSON_GetArrayItem(rcj, 0)->valuedouble, cJSON_GetArrayItem(rcj, 1)->valuedouble, cJSON_GetArrayItem(rcj, 2)->valuedouble }};
    

    // where is the hand?
    allo_m4x4 parent_from_grabber_transform = entity_get_transform(grabber);
    allo_m4x4 world_from_grabber_transform = state_convert_coordinate_space(state, parent_from_grabber_transform, entity_get_parent(state, grabber), NULL);

    // where in world should grabbed be?
    allo_m4x4 world_from_grabbed_transform = allo_m4x4_concat(world_from_grabber_transform, grab->grabber_from_entity_transform);

    // consequently, where should ACTUATED be?
    allo_m4x4 grabbed_from_actuated_transform = state_convert_coordinate_space(state, allo_m4x4_identity(), actuated, grabbed);
    allo_m4x4 world_from_actuated = allo_m4x4_concat(world_from_grabbed_transform, grabbed_from_actuated_transform);

    // okay but this is a scenegraph -- in grabbed PARENT'S coordinate space, where is it?
    allo_m4x4 parent_from_actuated_transform = state_convert_coordinate_space(state, world_from_actuated, NULL, entity_get_parent(state, actuated));

    // constrain to only requested axes and rotations
    allo_m4x4 constrained = _constrain(entity_get_transform(actuated), parent_from_actuated_transform, translation_constraint, rotation_constraint);

    entity_set_transform(actuated, constrained);
  }
}

// picks translation and rotation from 'update' and 'orig' according to the fractions in translation constraint and rotation constraint.
static allo_m4x4 _constrain(allo_m4x4 orig, allo_m4x4 update, allo_vector tconstraint, allo_vector rconstraint)
{
  // figure out a new translation that picks the axes from 'update' that are >0 in tconstraint, and keeps the rest from 'orig'
  allo_vector oldT = allo_m4x4_get_position(orig);
  allo_vector newT = allo_m4x4_get_position(update);
  allo_vector tconstraint_inv = allo_vector_subtract((allo_vector) {{ 1, 1, 1 }}, tconstraint);
  allo_vector constrainedTranslation = allo_vector_add(allo_vector_mul(oldT, tconstraint_inv), allo_vector_mul(newT, tconstraint));

  // not sure how to do the same for rotation. let's pretend the axis-angle pair can be cherry-picked from; it sorta-kinda works at least.
  allo_rotation oldR = allo_m4x4_get_rotation(orig);
  allo_rotation newR = allo_m4x4_get_rotation(update);
  allo_vector rconstraint_inv = allo_vector_subtract((allo_vector) {{ 1, 1, 1 }}, rconstraint);
  allo_vector constrainedAxis = allo_vector_add(allo_vector_mul(oldR.axis, rconstraint_inv), allo_vector_mul(newR.axis, rconstraint));

  // concat for a new and fresh transform for our entity!
  return allo_m4x4_concat(allo_m4x4_translate(constrainedTranslation), allo_m4x4_rotate(newR.angle, constrainedAxis));
}
