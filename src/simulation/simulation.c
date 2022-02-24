#include "simulation.h"

void allo_simulate(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff)
{
  // wait until we have received first state before simulating
  if(
    state->state == NULL || 
    Alloverse_Entity_vec_find_by_id(Alloverse_State_entities_get(state->state), "place") == flatbuffers_not_found
  ) return;

  // figure out what time was in pre-sim state
  double old_time = state_set_server_time(state, server_time);
  // TODO: figure out what to send for Components
  allo_state_diff_mark_component_updated(diff, "place", "clock", NULL);
  
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
    const allo_entity* head = allosim_get_child_with_pose(state, avatar, "head");
    allosim_stick_movement(avatar, head, intent, dt, true, diff);
    allosim_pose_movements(state, avatar, intent, intents, intent_count, dt, diff);
    allosim_handle_grabs(state, avatar, intent, dt, diff);
  }

  allosim_animate(state, server_time, diff);
}

const allo_entity* allosim_get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name)
{
  if (!state || !avatar || !pose_name || strlen(pose_name) == 0)
    return NULL;
  
  const char *avatar_id = Alloverse_Entity_id_get(avatar);
  Alloverse_Entity_vec_t entities = Alloverse_State_entities_get(state->state);

  for(int i = 0, c = Alloverse_Entity_vec_len(entities); i < c; i++)
  {
    const allo_entity* entity = Alloverse_Entity_vec_at(entities, i);
    const Alloverse_Components_table_t comps = Alloverse_Entity_components_get(entity);
    const Alloverse_RelationshipsComponent_table_t relationships = Alloverse_Components_relationships_get(comps);
    const char *parent = relationships ? Alloverse_RelationshipsComponent_parent_get(relationships) : NULL;
    const Alloverse_IntentComponent_table_t intent = Alloverse_Components_intent_get(comps);
    const char *actuate_pose = intent ? Alloverse_IntentComponent_actuate_pose_get(intent) : NULL;

    if (parent && actuate_pose && strcmp(parent, avatar_id) == 0 && strcmp(actuate_pose, pose_name) == 0)
    {
      return entity;
    }
  }
  return NULL;
}
