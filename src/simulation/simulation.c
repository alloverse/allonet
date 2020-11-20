#include "simulation.h"

void allo_simulate(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time)
{
  // figure out what time was in pre-sim state
  allo_entity *place = state_get_entity(state, "place");
  double old_time = 0.0;
  if(place) {
    cJSON *clock = cJSON_GetObjectItem(place->components, "clock");
    if(!clock) {
      clock = cjson_create_object("time", cJSON_CreateNumber(server_time));
      cJSON_AddItemToObject(place->components, "clock", clock);
    }
    cJSON *time =cJSON_GetObjectItem(clock, "time");
    old_time = time->valuedouble;
    cJSON_SetNumberValue(time, server_time);
  }
  // todo: run simulate at fixed-sized steps
  // https://gafferongames.com/post/fix_your_timestep/
  // https://www.gamasutra.com/blogs/BramStolk/20160408/269988/Fixing_your_time_step_the_easy_way_with_the_golden_48537_ms.php
  // for now, slow down simulation if we're given a larger dt than a 20fps equivalent
  double dt = server_time - old_time;
  dt = MIN(dt, 1/20.0);

  allo_simulate_iteration(state, intents, intent_count, dt);
}

void allo_simulate_iteration(allo_state* state, const allo_client_intent* intents[], int intent_count, double dt)
{
  for (int i = 0; i < intent_count; i++)
  {
    const allo_client_intent *intent = intents[i];
    allo_entity* avatar = state_get_entity(state, intent->entity_id);
    if (intent->entity_id == NULL || avatar == NULL)
      return;
    allo_entity* head = allosim_get_child_with_pose(state, avatar, "head");
    allosim_stick_movement(avatar, head, intent, dt);
    allosim_pose_movement(state, avatar, intent, intents, intent_count, dt);
    allosim_handle_grabs(state, avatar, intent, dt);
  }
}

allo_entity* allosim_get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name)
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
