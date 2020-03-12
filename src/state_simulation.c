#include "allonet/state.h"


static void move_avatars(allo_state* state, double dt, allo_client_intent* intents, int intent_count);
static void move_poses(allo_state* state, double dt, allo_client_intent* intents, int intent_count);

extern void allo_simulate(allo_state* state, double dt, allo_client_intent* intents, int intent_count)
{
  move_avatars(state, dt, intents, intent_count);
  move_poses(state, dt, intents, intent_count);
}

static void move_avatar(allo_entity* ent, allo_client_intent intent, double dt)
{
  double speed = 1.0; // meters per second
  double distance = speed * dt;

  allo_m4x4 old_transform = entity_get_transform(ent);

  // intent movement is always relative to the facing direction of the user, controlled
  // by the yaw intent.
  // Begin by creating a matrix representing that yaw and one for user-relative translation...
  allo_m4x4 rotation = allo_m4x4_rotate(intent.yaw, (allo_vector) { 0, 1, 0 });
  allo_m4x4 translation = allo_m4x4_translate((allo_vector) { intent.xmovement* distance, 0, intent.zmovement* distance });
  // then combine rotation and translation to create a movement matrix,
  allo_m4x4 movement = allo_m4x4_concat(rotation, translation);

  // which can then be concat'd into the old transform.
  allo_vector old_position = allo_m4x4_transform(old_transform, (allo_vector) { 0, 0, 0 });
  allo_m4x4 old_positional_transform = allo_m4x4_translate(old_position);

  allo_m4x4 new_transform = allo_m4x4_concat(old_positional_transform, movement);

  entity_set_transform(ent, new_transform);
}

static void move_avatars(allo_state* state, double dt, allo_client_intent* intents, int intent_count)
{
  for (int i = 0; i < intent_count; i++) 
  {
    allo_client_intent intent = intents[i];
    allo_entity *ent = state_get_entity(state, intent.entity_id);
    if (intent.entity_id == NULL || ent == NULL)
      return;
    
    move_avatar(ent, intent, dt);
  }
}

static void move_poses(allo_state* state, double dt, allo_client_intent* intents, int intent_count)
{

}

