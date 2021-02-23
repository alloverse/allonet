#include "simulation.h"

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

void allosim_handle_grabs(allo_state *const state, allo_entity *const avatar, const allo_client_intent *const intent, double dt)
{
  (void)dt;
  const allo_client_pose_grab* grabs[] = { &intent->poses.left_hand.grab, &intent->poses.right_hand.grab };
  allo_entity* grabbers[] = { allosim_get_child_with_pose(state, avatar, "hand/left"), allosim_get_child_with_pose(state, avatar, "hand/right") };
  for (int i = 0; i < 2; i++)
  {
    const allo_client_pose_grab* grab = grabs[i];
    allo_entity *const grabber = grabbers[i];
    allo_entity *const grabbed = state_get_entity(state, grab->entity);
    allo_entity * actuated = grabbed;
    cJSON *const grabbable = cJSON_GetObjectItemCaseSensitive(grabbed ? grabbed->components : NULL, "grabbable");
    const char* actuate_on = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(grabbable, "actuate_on"));
    if (actuate_on && strlen(actuate_on) > 0) {
      if (strcmp(actuate_on, "$parent") == 0) {
        actuated = entity_get_parent(state, grabbed);
      } else {
        actuated = state_get_entity(state, actuate_on);
      }
    }
    if (!grab || !grabber || !grabbed || !actuated || !grabbable) { continue; }

    cJSON* const trcj = cJSON_GetObjectItemCaseSensitive(grabbable, "translation_constraint");
    allo_vector translation_constraint = {{ 1,1,1 }};
    if (cJSON_GetArraySize(trcj) == 3)
      translation_constraint = (allo_vector){{ cJSON_GetArrayItem(trcj, 0)->valuedouble, cJSON_GetArrayItem(trcj, 1)->valuedouble , cJSON_GetArrayItem(trcj, 2)->valuedouble }};
    cJSON* const rcj = cJSON_GetObjectItemCaseSensitive(grabbable, "rotation_constraint");
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

