#include <allonet/state.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <cJSON/cJSON.h>
#include "../util.h"

#ifdef __cplusplus
#include <unordered_map>
#include <string>
#include <memory>

namespace Alloverse { 
    
    class Entity; 

    struct SimulationCache {
        std::unordered_map<std::string, std::shared_ptr<AlloPropertyAnimation> > animations;    
    };
}

struct AlloPropertyAnimation;


extern "C" {
#endif

extern void allo_simulate_iteration(allo_state* state, Alloverse::SimulationCache *cache, const allo_client_intent* intents[], int intent_count, double server_time, double dt, allo_state_diff *diff);
extern allo_m4x4 allosim_stick_movement(Alloverse::Entity* avatar, Alloverse::Entity* head, const allo_client_intent *intent, double dt, bool write, allo_state_diff *diff);
extern void allosim_pose_movements(allo_state* state, Alloverse::Entity* avatar, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt, allo_state_diff *diff);
extern void allosim_handle_grabs(allo_state *state, Alloverse::Entity *avatar, const allo_client_intent *intent, double dt, allo_state_diff *diff);
extern void allosim_animate(allo_state *state, Alloverse::SimulationCache *cache, double server_time, allo_state_diff *diff);

#ifdef __cplusplus
}

extern Alloverse::Entity* allosim_get_child_with_pose(allo_state* state, const char* avatar_id, const char* pose_name);
#endif
