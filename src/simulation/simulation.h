#include <allonet/state.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <cJSON/cJSON.h>
#include "../util.h"


extern void allo_simulate_iteration(allo_state* state, const allo_client_intent* intents[], int intent_count, double dt);
extern void allosim_stick_movement(allo_entity* avatar, allo_entity* head, const allo_client_intent *intent, double dt);
extern void allosim_pose_movement(allo_state* state, allo_entity* ent, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt);
extern void allosim_handle_grabs(allo_state *state, allo_entity *avatar, const allo_client_intent *intent, double dt);
extern allo_entity* allosim_get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name);

