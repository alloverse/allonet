#ifndef ALLONET_STATE_DIFF_H
#define ALLONET_STATE_DIFF_H
#include "math.h"
#include <stdint.h>
#include <stdbool.h>
#include <allonet/arr.h>
#include <cJSON/cJSON.h>
struct allo_state;

#ifdef __cplusplus
extern "C" {
#endif

typedef arr_t(const char*) allo_entity_id_vec;

// a reference to a specific component in a specific entity. 
// component named `name` in entity with id `eid.
typedef struct allo_component_ref
{
    // these are all non-owning references into the accompanying state,
    // so they're only valid for as long as you hold onto the state
    const char *eid;
    const char *name;
    // pointer to flatbuffer for this component in oldstate
    const void *olddata;
    // pointer to flatbuffer for this component in newstate
    const void *newdata;
} allo_component_ref;

typedef arr_t(allo_component_ref) allo_component_vec;

typedef struct allo_state_diff
{
    /// List of entities that have been created since last callback
    allo_entity_id_vec new_entities;
    /// List of entities that have disappeared since last callback
    allo_entity_id_vec deleted_entities;
    /// List of components that have been created since last callback, including any components of entities that just appeared.
    allo_component_vec new_components;
    /// List of components that have had one or more values changed
    allo_component_vec updated_components;
    /// List of components that have disappeared since last callback, including components of recently deleted entities.
    allo_component_vec deleted_components;
} allo_state_diff;

/// initializes the fields in the diff to zero-length lists
extern void allo_state_diff_init(allo_state_diff *diff);

/// calculates the difference in entities and components between oldstate and newstate
extern void allo_state_diff_compute(allo_state_diff *diff, struct allo_state *oldstate, struct allo_state *newstate);

/// duplicates an existing diff and repoints all refs to point into `newstate` instead of `oldstate`.
extern allo_state_diff *allo_state_diff_duplicate(allo_state_diff *orig, struct allo_state *oldstate, struct allo_state *newstate); 

extern void allo_state_diff_destroy(allo_state_diff *diff);
extern void allo_state_diff_dump(allo_state_diff *diff);
extern void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const void *comp);
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const void *comp);


#ifdef __cplusplus
}
#endif

#endif
