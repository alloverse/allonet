#include <cJSON/cJSON.h>
#include <cJSON/cJSON_Utils.h>
#include <stdint.h>
#include <stdbool.h>

#define allo_statehistory_length 64
typedef struct statehistory_t
{
    cJSON *history[allo_statehistory_length];
    int64_t latest_revision;
} statehistory_t;

/// Add a new state to the history. You relinquish ownership and must not touch the state afterwards.
extern void allo_delta_insert(statehistory_t *history, cJSON *next_state);
/// Destroy all cached states (but not the history pointer itself; that's on you)
extern void allo_delta_destroy(statehistory_t *history);
/// Return a state delta that can be transmitted to a client, who can later merge it with their old_revision.
/// You own the returned memory and must free it.
extern char* allo_delta_compute(statehistory_t *history, int64_t old_revision);

/// In a receiving client, apply delta to current. Returns false if it has failed because
/// it was corrupt. `current` is likely also corrupt at that point :S not sure what to do about that.
/// Probably send ack_state_rev=0 and wait for a new full state.
extern bool allo_delta_apply(cJSON *current, cJSON *delta);
