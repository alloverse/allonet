#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#include <inlinesys/queue.h>
#include "math.h"
#include <stdint.h>
#include <stdbool.h>
#include <cJSON/cJSON.h>
#include <allonet/arr.h>
#include <allonet/schema/alloverse_reader.h>
#include <allonet/schema/alloverse_builder.h>
#include <allonet/schema/alloverse_verifier.h>

#pragma pack(push)
#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct allo_client_pose_grab
{
    char* entity; // which entity is being grabbed. null = none
    allo_m4x4 grabber_from_entity_transform;
} allo_client_pose_grab;

#define ALLO_HAND_SKELETON_JOINT_COUNT 26
typedef struct allo_client_hand_pose
{
    allo_m4x4 matrix;
    allo_m4x4 skeleton[ALLO_HAND_SKELETON_JOINT_COUNT];
    allo_client_pose_grab grab;
} allo_client_hand_pose;

typedef struct allo_client_plain_pose
{
    allo_m4x4 matrix;
} allo_client_plain_pose;

typedef struct allo_client_poses
{
    allo_client_plain_pose root;
    allo_client_plain_pose head;
    allo_client_plain_pose torso;
    allo_client_hand_pose left_hand;
    allo_client_hand_pose right_hand;
} allo_client_poses;

typedef struct allo_client_intent
{
    char* entity_id; // which entity is this intent for
    bool wants_stick_movement;
    double zmovement; // 1 = maximum speed forwards
    double xmovement; // 1 = maximum speed strafe right
    double yaw;       // rotation around x in radians
    double pitch;     // rotation around y in radians
    allo_client_poses poses;
    uint64_t ack_state_rev;
} allo_client_intent;

extern allo_client_intent *allo_client_intent_create();
extern void allo_client_intent_free(allo_client_intent* intent);
extern void allo_client_intent_clone(const allo_client_intent* original, allo_client_intent* destination);
extern cJSON* allo_client_intent_to_cjson(const allo_client_intent *intent);
extern allo_client_intent *allo_client_intent_parse_cjson(const cJSON* from);

// generate an identifier of 'len'-1 chars, and null the last byte in str.
extern void allo_generate_id(char *str, size_t len);

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
extern void allo_state_diff_init(allo_state_diff *diff);
extern void allo_state_diff_free(allo_state_diff *diff);
extern void allo_state_diff_dump(allo_state_diff *diff);
extern void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp);
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp);

extern void allo_state_to_flat(allo_state *state, flatcc_builder_t *B);
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
extern allo_interaction *allo_interaction_clone(const allo_interaction *interaction);
extern void allo_interaction_free(allo_interaction *interaction);

/**
 * Initialize the Allonet library. Must be called before any other Allonet calls.
 */
extern bool allo_initialize(bool redirect_stdout);
/**
 * If you're linking liballonet_av, you also have to initialize that sub-library.
 */
extern void allo_libav_initialize(void);

/**
 * Run world simulation for a given state and known intents. Modifies state inline.
 * Will run the number of world iterations needed to get to server_time (or skip if too many)
 */
extern void allo_simulate(allo_state* state, const allo_client_intent* intents[], int intent_count, double server_time, allo_state_diff *diff);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif
