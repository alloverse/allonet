#ifndef ALLONET_STATE_WRITE_H
#define ALLONET_STATE_WRITE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <allonet/state/state_read.h>

extern void allo_state_init(allo_state *state);
extern void allo_state_destroy(allo_state *state);


// create a new entity in state with the given ID. Might reallocate to make room for it in state->flat.
allo_entity *entity_create(allo_state *state, const char *id);
// remove the given entity from state, free its slot, and free and associated resources.
void entity_destroy(allo_state *state, allo_entity *entity);

typedef enum allo_removal_mode
{
    AlloRemovalCascade,
    AlloRemovalReparent,
} allo_removal_mode;

/// Add a new entity to the state based on a JSON specification of its components.
/// @param agent_id: Arbitrary string representing the client that owns this entity. Only used server-side. strdup'd.
/// @param spec: JSON with components. also key "children" with nested json of same structure. 
///             NOTE!! this reference is stolen, so you must not reference or free it!
/// @param parent: entity ID of parent. will create "relationships" component if set.
extern allo_entity* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent);
extern bool allo_state_remove_entity_id(allo_state *state, const char *eid, allo_removal_mode mode);
extern bool allo_state_remove_entity(allo_state *state, allo_entity *removed_entity, allo_removal_mode mode);



#ifdef __cplusplus
}
#endif
#endif