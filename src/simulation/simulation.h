#include <allonet/state.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <cJSON/cJSON.h>
#include "../util.h"


extern void allo_simulate_iteration(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, double dt, allo_state_diff *diff);
extern allo_m4x4 allosim_stick_movement(allo_entity* avatar, allo_entity* head, const allo_client_intent *intent, double dt, bool write, allo_state_diff *diff);
extern void allosim_pose_movements(allo_state* state, allo_entity* avatar, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt, allo_state_diff *diff);
extern void allosim_handle_grabs(allo_state *state, allo_entity *avatar, const allo_client_intent *intent, double dt, allo_state_diff *diff);
extern allo_entity* allosim_get_child_with_pose(allo_state* state, allo_entity* avatar, const char* pose_name);
extern void allosim_animate(allo_state *state, double server_time, allo_state_diff *diff);
