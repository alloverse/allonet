#ifndef ALLONET_STATE_READ_H
#define ALLONET_STATE_READ_H
#include <inlinesys/queue.h>
#include <cJSON/cJSON.h>
#include <allonet/math.h>
#include <allonet/schema/alloverse_reader.h>
#include <allonet/schema/alloverse_builder.h>
#include <allonet/schema/alloverse_verifier.h>
#if defined(__cplusplus) && defined(ALLO_INTERNALS)
namespace Alloverse { class State; }
#endif
#ifdef __cplusplus
extern "C" {
#endif

// This file has the "mostly read-only" world state API that clients use to interpret
// the world. A separate "write" API is available for server-side modification of the world.
// Clients can only modify the world by asking the server for an appropriate modification.
// There is no client-side mutable state (except for declarative interpolation in the
// animation API)

/// Full representation of the world, 
typedef struct allo_state
{
    /// the buffer containing the full state
    size_t flatlength;
    void *flat;

    // parsed world state
    Alloverse_State_table_t state;
    /// parsed revision from buffer
    uint64_t revision;

    // internal parsed cpp version of 'state'
#if defined(__cplusplus) && defined(ALLO_INTERNALS)
    const Alloverse::State *_cur;
#else
    void *_cur;
#endif

    // Only to be able to compile...
    LIST_HEAD(allo_entity_list, allo_entity) entities;
} allo_state;

//typedef struct Alloverse_Entity_table allo_entity;
typedef struct allo_entity{
    char *id;
    char *owner_agent_id;
    cJSON *components;
    LIST_ENTRY(allo_entity) pointers;
} allo_entity;

extern void allo_state_create_parsed(allo_state *state, void *buf, size_t len);
extern void allo_state_destroy(allo_state *state);


// generate an identifier of 'len'-1 chars into str, and null the last byte in str.
extern void allo_generate_id(char *str, size_t len);

extern allo_entity* state_get_entity(allo_state* state, const char* entity_id);
extern allo_entity* entity_get_parent(allo_state* state, allo_entity* entity);


// parse and get the transform-from-parent for the entity
extern allo_m4x4 entity_get_transform(allo_entity* entity);
// update the transform stored in the entity. this is a mutation, but remains in the "read"
// api since it can be done in-place and is used to interpolate local state in the client.
extern void entity_set_transform(allo_entity* entity, allo_m4x4 matrix);
extern allo_m4x4 entity_get_transform_in_coordinate_space(allo_state* state, allo_entity* entity, allo_entity* space);
extern allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* from_space, allo_entity* to_space);

#ifdef __cplusplus
}
#endif
#endif
