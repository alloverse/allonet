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

/// Add a new state to the history. You relinquish ownership and may only use it until the
/// next call of allo_delta_* (and you sould definitely not modify it).
extern void allo_delta_insert(statehistory_t *history, cJSON *next_state);
/// Destroy all cached states (but not the history pointer itself; that's on you)
extern void allo_delta_clear(statehistory_t *history);
/// Return a state delta that can be transmitted to a client, who can later merge it with their old_revision.
/// You own the returned memory and must free it.
extern char* allo_delta_compute(statehistory_t *history, int64_t old_revision);

/// In a receiving client, apply delta to something in history.
/// This call takes ownership of delta, and frees it when needed.
/// If successful, returns new full state and also inserts it into history.
/// If delta is corrupt or a patch for a state we don't have, returns false. In thiis case,
/// you should send intent.ack_state_rev=0 to get a new full state.
/// The returned cJSON has the same restrictions as one that is allo_delta_inserted.
extern cJSON *allo_delta_apply(statehistory_t *history, cJSON *delta);

extern cJSON *statehistory_get(statehistory_t *history, int64_t revision);
