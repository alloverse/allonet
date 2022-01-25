#include <cJSON/cJSON.h>
#include <cJSON/cJSON_Utils.h>
#include <stdint.h>
#include <stdbool.h>
#include <allonet/state.h>

#define allo_statehistory_length 64
typedef struct statehistory_t
{
    cJSON *history[allo_statehistory_length];
    int64_t latest_revision;
} statehistory_t;

typedef void (*allo_statediff_handler)(void *userinfo, cJSON *cmd, cJSON *staterep, allo_state_diff *diff);

/// Add a new state to the history. You relinquish ownership and may only use it until the
/// next call of allo_delta_* (and you sould definitely not modify it).
extern void allo_delta_insert(statehistory_t *history, cJSON *next_state);
/// Destroy all cached states (but not the history pointer itself; that's on you)
extern void allo_delta_clear(statehistory_t *history);
/// Return a state delta that can be transmitted to a client, who can later merge it with their old_revision.
/// You own the returned memory and must free it.
extern char* allo_delta_compute(statehistory_t *history, int64_t old_revision);

/** In a receiving client, apply delta to something in history.
 * This call takes ownership of delta, and frees it when needed.
 * If successful, returns new full state, inserts it into history, and gives you a state_diff
 * so you know what has changed in the world.
 * If delta is corrupt or a patch for a state we don't have, returns false. In this case,
 * you should send intent.ack_state_rev=0 to get a new full state.
 * The returned cJSON has the same restrictions as one that is allo_delta_inserted.
 * @param history   History of states, managed internally. You just have to allocate one and sending it to apply
 * @param delta     The incoming changeset json, from network
 * @param diff      the difference from the previous state in history will be put in this. This is for you to use
 *                  to react to the changes in state. Send NULL if you don't care.
 * @param handler   'diff' will only be valid during this callback
 * @param userinfo  for the handler, so you know where you came from
 */
extern cJSON *allo_delta_apply(statehistory_t *history, cJSON *delta, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo);

extern cJSON *statehistory_get(statehistory_t *history, int64_t revision);
extern cJSON *statehistory_get_latest(statehistory_t *history);
