#include "simulation.h"

static allo_m4x4 create_movement(allo_m4x4 heading_transform, double yaw, double xmovement, double zmovement)
{
  allo_m4x4 inverse_head = allo_m4x4_inverse(heading_transform);
  // intent movement is always relative to the facing direction of the user, controlled
  // by the head transform and yaw intent.
  // Begin by creating a matrix representing that yaw and one for user-relative translation...
  allo_m4x4 rotation = allo_m4x4_rotate(yaw, (allo_vector) {{ 0, -1, 0 }});
  allo_m4x4 translation = allo_m4x4_translate((allo_vector) {{ xmovement, 0, zmovement }});
  // then combine head transform, rotation and translation to create a movement matrix,
  allo_m4x4 full_movement = allo_m4x4_concat(allo_m4x4_concat(allo_m4x4_concat(rotation, heading_transform), translation), inverse_head);

  // then compensate for any non-yaw rotation of the heading which accidentally makes us ascend or descend
  allo_vector origin = { {0, 0, 0} };
  allo_vector movement_vector = allo_m4x4_transform(full_movement, origin, true);
  allo_m4x4 constrainer = allo_m4x4_translate((allo_vector) { {0, -movement_vector.y, 0} });
  allo_m4x4 constrained_movement = allo_m4x4_concat(full_movement, constrainer);
  return constrained_movement;
}

allo_m4x4 allosim_stick_movement(allo_entity* avatar, allo_entity* head, const allo_client_intent *intent, double dt, bool write)
{
  double speed = 2.0; // meters per second
  double distance = speed * dt;

  allo_m4x4 old_transform = entity_get_transform(avatar);
  allo_m4x4 heading_transform = entity_get_transform(head);
  allo_m4x4 movement = create_movement(heading_transform, intent->yaw, intent->xmovement * distance, intent->zmovement * distance);
  
  // which can then be concat'd into the old transform.
  allo_vector old_position = allo_m4x4_transform(old_transform, (allo_vector){{ 0, 0, 0 }}, true);
  allo_m4x4 old_positional_transform = allo_m4x4_translate(old_position);

  // now we gotta compensate: rotating the avatar will MOVE the head if it's not in origin, so we'll have to move
  // the avatar so the head stays in place.
  allo_m4x4 head_worldcoords = allo_m4x4_concat(old_transform, heading_transform);

  allo_m4x4 just_rotation = create_movement(heading_transform, intent->yaw, 0, 0);
  allo_m4x4 new_transform_r = allo_m4x4_concat(old_positional_transform, just_rotation);
  allo_m4x4 new_head_worldcoords = allo_m4x4_concat(new_transform_r, heading_transform);
  allo_vector a = allo_m4x4_transform(head_worldcoords, (allo_vector){{ 0, 0, 0 }}, true);
  allo_vector b = allo_m4x4_transform(new_head_worldcoords, (allo_vector){{ 0, 0, 0 }}, true);
  allo_vector head_movement = allo_vector_subtract(a, b);
  allo_m4x4 keep_head_centered = allo_m4x4_translate(head_movement);

  // Now calculate the new transform for the root entity
  allo_m4x4 new_transform = allo_m4x4_concat(old_positional_transform, movement);
  allo_m4x4 new_transform2 = allo_m4x4_concat(keep_head_centered, new_transform);

  if(write)
    entity_set_transform(avatar, new_transform2);
  
  return new_transform2;
}
