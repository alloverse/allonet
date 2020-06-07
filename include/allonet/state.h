#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#include "inlinesys/queue.h"
#include "allonet/math.h"
#include <stdint.h>
#include <stdbool.h>
#include <cJSON/cJSON.h>

#pragma pack(push)
#pragma pack(1)

typedef struct allo_client_pose_grab
{
    char* entity; // which entity is being grabbed. null = none
    allo_vector held_at; // where in the grabbed entity's coordinate space is it held
} allo_client_pose_grab;

typedef struct allo_client_pose
{
    allo_m4x4 matrix;
    allo_client_pose_grab grab;
} allo_client_pose;

typedef struct allo_client_poses
{
    allo_client_pose head;
    allo_client_pose left_hand;
    allo_client_pose right_hand;
} allo_client_poses;

typedef struct allo_client_intent
{
    char* entity_id; // which entity is this intent for
    double zmovement; // 1 = maximum speed forwards
    double xmovement; // 1 = maximum speed strafe right
    double yaw;       // rotation around x in radians
    double pitch;     // rotation around y in radians
    allo_client_poses poses;
} allo_client_intent;

extern allo_client_intent *allo_client_intent_create();
extern void allo_client_intent_free(allo_client_intent* intent);
extern void allo_client_intent_clone(const allo_client_intent* original, allo_client_intent* destination);
extern cJSON* allo_client_intent_to_cjson(const allo_client_intent *intent);
extern allo_client_intent *allo_client_intent_parse_cjson(const cJSON* from);

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

allo_entity *entity_create(const char *id);
void entity_destroy(allo_entity *entity);

extern allo_m4x4 entity_get_transform(allo_entity* entity);
extern void entity_set_transform(allo_entity* entity, allo_m4x4 matrix);

typedef struct allo_state
{
    uint64_t revision;
    LIST_HEAD(allo_entity_list, allo_entity) entities;
} allo_state;

extern allo_entity* allo_state_add_entity_from_spec(allo_state* state, const char* agent_id, cJSON* spec, const char* parent);
extern allo_entity* state_get_entity(allo_state* state, const char* entity_id);
extern allo_entity* entity_get_parent(allo_state* state, allo_entity* entity);
extern allo_m4x4 entity_get_transform_in_coordinate_space(allo_state* state, allo_entity* entity, allo_entity* space);
extern allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, allo_entity* from_space, allo_entity* to_space);

/**
 * Describes an interaction to be sent or as received.
 * @field type: oneway, request, response or publication
 * @field sender_entity_id: the entity trying to interact with yours
 * @field receiver_entity_id: your entity being interacted with
 * @field request_id: The ID of this request or response
 * @field body: JSON list of interaction message
 */
typedef struct allo_interaction
{
    const char *type;
    const char *sender_entity_id;
    const char *receiver_entity_id;
    const char *request_id;
    const char *body;
} allo_interaction;

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body);
extern cJSON* allo_interaction_to_cjson(const allo_interaction* interaction);
extern allo_interaction *allo_interaction_parse_cjson(const cJSON* from);
void allo_interaction_free(allo_interaction *interaction);

/**
 * Initialize the Allonet library. Must be called before any other Allonet calls.
 */
extern bool allo_initialize(bool redirect_stdout);

/**
 * Run world simulation for a given state and known intents. Modifies state inline.
 */
extern void allo_simulate(allo_state* state, double dt, const allo_client_intent** intents, int intent_count);

#pragma pack(pop)
#endif
