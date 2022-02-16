#ifndef ALLONET_STATE_DIFF_H
#define ALLONET_STATE_DIFF_H
#include "math.h"
#include <stdint.h>
#include <stdbool.h>
#include <allonet/arr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef arr_t(const char*) allo_entity_id_vec;

typedef struct allo_component_ref
{
    const char *eid;
    const char *name;
    const cJSON *olddata;
    const cJSON *newdata;
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

extern void allo_state_diff_init(allo_state_diff *diff);
extern void allo_state_diff_free(allo_state_diff *diff);
extern void allo_state_diff_dump(allo_state_diff *diff);
extern void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp);
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp);


#ifdef __cplusplus
}
#endif

#endif
