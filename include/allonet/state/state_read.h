#ifndef ALLONET_STATE_READ_H
#define ALLONET_STATE_READ_H
#ifdef __cplusplus
extern "C" {
#endif

#include <inlinesys/queue.h>
#include <allonet/schema/alloverse_reader.h>
#include <allonet/schema/alloverse_builder.h>
#include <allonet/schema/alloverse_verifier.h>


typedef struct allo_entity
{
    // Place-unique ID for this entity
    char *id;
    // Place's server-side ID for the agent that owns this entity
    char *owner_agent_id;

    // exposing implementation detail json isn't _great_ but best I got atm.
    // See https://github.com/alloverse/docs/blob/master/specifications/components.md for official
    // contained values
    cJSON *components;

    LIST_ENTRY(allo_entity) pointers;
} allo_entity;


// generate an identifier of 'len'-1 chars, and null the last byte in str.
extern void allo_generate_id(char *str, size_t len);




allo_entity *entity_create(const char *id);
void entity_destroy(allo_entity *entity);

extern allo_m4x4 entity_get_transform(allo_entity* entity);
extern void entity_set_transform(allo_entity* entity, allo_m4x4 matrix);

typedef struct allo_state
{
    uint64_t revision;
    LIST_HEAD(allo_entity_list, allo_entity) entities;
} allo_state;

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
extern allo_entity* state_get_entity(allo_state* state, const char* entity_id);
extern allo_entity* entity_get_parent(allo_state* state, allo_entity* entity);
extern allo_m4x4 entity_get_transform_in_coordinate_space(allo_state* state, allo_entity* entity, allo_entity* space);
extern allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* from_space, allo_entity* to_space);
extern void allo_state_init(allo_state *state);
extern void allo_state_destroy(allo_state *state);
extern cJSON *allo_state_to_json(allo_state *state, bool include_agent_id);
extern allo_state *allo_state_from_json(cJSON *state);

extern void allo_state_to_flat(allo_state *state, flatcc_builder_t *B);

#ifdef __cplusplus
}
#endif
#endif