#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <allonet/state/intent.h>
#include <allonet/state/diff.h>
#include <allonet/state/interaction.h>
#include <allonet/state/state_read.h>


/**
 * Initialize the Allonet library. Must be called before any other Allonet calls.
 */
extern bool allo_initialize(bool redirect_stdout);
/**
 * If you're linking liballonet_av, you also have to initialize that sub-library.
 */
extern void allo_libav_initialize(void);

/**
 * Run world simulation for a given state and known intents. Modifies state inline.
 * Will run the number of world iterations needed to get to server_time (or skip if too many)
 */
extern void allo_simulate(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff);

#ifdef __cplusplus
}
#endif
#endif
