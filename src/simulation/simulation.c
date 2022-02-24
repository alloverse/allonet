#include "simulation.h"

void allo_simulate(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff)
{
  // figure out what time was in pre-sim state
  state_set_server_time(state, server_time);
  allo_state_diff_mark_component_updated(diff, "place", "clock", clock);
  
  // todo: run simulate at fixed-sized steps
  // https://gafferongames.com/post/fix_your_timestep/
  // https://www.gamasutra.com/blogs/BramStolk/20160408/269988/Fixing_your_time_step_the_easy_way_with_the_golden_48537_ms.php
  // for now, slow down simulation if we're given a larger dt than a 20fps equivalent
  double dt = server_time - old_time;
  dt = dt < 1/5.0 ? dt : 1/5.0;
  allo_simulate_iteration(state, intents, intent_count, server_time, dt, diff);
}

void allo_simulate_iteration(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, double dt, allo_state_diff *diff)
{
  for (int i = 0; i < intent_count; i++)
  {
    const allo_client_intent *intent = intents[i];
    allo_entity* avatar = state_get_entity(state, intent->entity_id);
    if (intent->entity_id == NULL || avatar == NULL)
      return;
    allo_entity* head = allosim_get_child_with_pose(state, avatar, "head");
    allosim_stick_movement(avatar, head, intent, dt, true, diff);
    allosim_pose_movements(state, avatar, intent, intents, intent_count, dt, diff);
    allosim_handle_grabs(state, avatar, intent, dt, diff);
  }

  allosim_animate(state, server_time, diff);
}

allo_entity* allosim_get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name)
{
  if (!state || !avatar || !pose_name || strlen(pose_name) == 0)
    return NULL;
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* relationships = cJSON_GetObjectItemCaseSensitive(entity->components, "relationships");
    cJSON* parent = cJSON_GetObjectItemCaseSensitive(relationships, "parent");
    cJSON* intent = cJSON_GetObjectItemCaseSensitive(entity->components, "intent");
    cJSON* actuate_pose = cJSON_GetObjectItemCaseSensitive(intent, "actuate_pose");

    if (parent && parent->valuestring && actuate_pose && strcmp(parent->valuestring, avatar->id) == 0 && strcmp(actuate_pose->valuestring, pose_name) == 0)
    {
      return entity;
    }
  }
  return NULL;
}
