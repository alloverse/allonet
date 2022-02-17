#ifndef ALLONET_STATE_WRITE_H
#define ALLONET_STATE_WRITE_H

#include <allonet/state/state_read.h>

#define ALLO_INTERNALS 1
#if defined(__cplusplus) && defined(ALLO_INTERNALS)
#include "alloverse_generated.h"

struct allo_mutable_state : allo_state
{
public:
    allo_mutable_state();
    // internal mutable object tree, only valid on server
    Alloverse::StateT next;
};

// create a new entity in state with the given ID. Might reallocate to make room for it in state->flat.
Alloverse::EntityT *entity_create(allo_mutable_state *state, const char *id);
// remove the given entity from state, free its slot, and free and associated resources.
void entity_destroy(allo_mutable_state *state, Alloverse::EntityT *entity);

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
Alloverse::EntityT* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent);
bool allo_state_remove_entity_id(allo_state *state, const char *eid, allo_removal_mode mode);
bool allo_state_remove_entity(allo_state *state, allo_entity *removed_entity, allo_removal_mode mode);

#endif // _cplusplus && ALLO_INTERNALS
#endif // ALLONET_STATE_WRITE_H
