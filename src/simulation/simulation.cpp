#define ALLO_INTERNALS 1
#include "simulation.h"
#include "alloverse_generated.h"
using namespace Alloverse;

void allo_simulation_cache_create(void **cache)
{
    *cache = new SimulationCache();
}
void allo_simulation_cache_destroy(void **cache)
{
  delete *(SimulationCache**)cache;
}

void allo_simulate(allo_state* state, void *cache, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff)
{
  SimulationCache *simcache = (SimulationCache*)cache;

  // wait until we have received first state before simulating
  if(state->getMutableEntity("place") == NULL) return;

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
  allo_simulate_iteration(state, simcache, intents, intent_count, server_time, dt, diff);
}

void allo_simulate_iteration(allo_state* state, SimulationCache *cache, const allo_client_intent* intents[], int intent_count, double server_time, double dt, allo_state_diff *diff)
{
  for (int i = 0; i < intent_count; i++)
  {
    const allo_client_intent *intent = intents[i];
    Entity* avatar = state->getMutableEntity(intent->entity_id);
    if (intent->entity_id == NULL || avatar == NULL)
      return;
    Entity* head = allosim_get_child_with_pose(state, avatar->id()->c_str(), "head");
    allosim_stick_movement(avatar, head, intent, dt, true, diff);
    allosim_pose_movements(state, avatar, intent, intents, intent_count, dt, diff);
    allosim_handle_grabs(state, avatar, intent, dt, diff);
  }

  allosim_animate(state, cache, server_time, diff);
}

Entity* allosim_get_child_with_pose(allo_state* state, const char* avatar_id, const char* pose_name)
{
  if (!state || !avatar_id || !pose_name || strlen(pose_name) == 0)
    return NULL;
  
  auto entities = state->_cur->mutable_entities();

  for(int i = 0, c = entities->size(); i < c; i++)
  {
    auto entity = entities->GetMutableObject(i);
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

extern "C" void allosim_simulate_root_pose(allo_state *state, const char *avatar_id, float dt, allo_client_intent *intent)
{
    /*assert(state && avatar_id && intent);
    allo_entity *avatar = state_get_entity(state, avatar_id);
    allo_entity* head = allosim_get_child_with_pose(state, avatar, "head");
    intent->poses.root.matrix = allosim_stick_movement(avatar, head, intent, dt, false, NULL);
    intent->xmovement = 0;
    intent->zmovement = 0;*/
}