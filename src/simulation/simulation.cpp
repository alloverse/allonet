#define ALLO_INTERNALS 1
#include "simulation.h"
#include "alloverse_generated.h"
using namespace Alloverse;

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
    const Entity* avatar = state->_cur->entities()->LookupByKey(intent->entity_id);
    if (intent->entity_id == NULL || avatar == NULL)
      return;
    const allo_entity* head = allosim_get_child_with_pose(state, avatar->id()->c_str(), "head");
    allosim_stick_movement(avatar, head, intent, dt, true, diff);
    allosim_pose_movements(state, avatar, intent, intents, intent_count, dt, diff);
    allosim_handle_grabs(state, avatar, intent, dt, diff);
  }

  allosim_animate(state, server_time, diff);
}

const Entity* allosim_get_child_with_pose(allo_state* state, const char* avatar_id, const char* pose_name)
{
  if (!state || !avatar_id || !pose_name || strlen(pose_name) == 0)
    return NULL;
  
  auto entities = state->_cur->entities();

  for(int i = 0, c = entities->size(); i < c; i++)
  {
    auto entity = entities->Get(i);
    auto comps = entity->components();
    auto rels = comps->relationships();
    auto parent = rels ? rels->parent() : NULL;
    auto intent = comps->intent();
    auto actuate_pose = intent ? intent->actuate_pose() : NULL;

    if (parent && actuate_pose && strcmp(parent->c_str(), avatar_id) == 0 && strcmp(actuate_pose->c_str(), pose_name) == 0)
    {
      return entity;
    }
  }
  return NULL;
}
