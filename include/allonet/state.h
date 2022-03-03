#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#include <allonet/state/intent.h>
#include <allonet/state/diff.h>
#include <allonet/state/interaction.h>
#include <allonet/state/state_read.h>
#include <allonet/state/state_write.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Allonet library. Must be called before any other Allonet calls.
 */
extern bool allo_initialize(bool redirect_stdout);
/**
 * If you're linking liballonet_av, you also have to initialize that sub-library.
 */
extern void allo_libav_initialize(void);

extern void allo_simulation_cache_create(void **cache);
extern void allo_simulation_cache_destroy(void **cache);

/**
 * Run world simulation for a given state and known intents. Modifies state inline.
 * Will run the number of world iterations needed to get to server_time (or skip if too many)
 * @param state         World state to perform simulation in.
 * @param cache         Pointer to a cache created with allo_simulation_cache_create();
 * @param intents       List of all the connected clients' intents (or just localhosts if running on client)
 * @param intent_count  How many intents are in the intents[] list?
 * @param server_time   if on server, what's the current get_ts_monod()? if on client, what's the current alloclient_get_time()?
 * @param diff          an allocated diff set that will have the list of changes from this simulation iteration
 */
extern void allo_simulate(allo_state* state, void *cache, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff);


#ifdef __cplusplus
}
#endif
#endif
