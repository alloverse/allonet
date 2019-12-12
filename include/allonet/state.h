#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#include "inlinesys/queue.h"
#include <stdint.h>
#include <stdbool.h>
#include <cJSON/cJSON.h>

#pragma pack(push)
#pragma pack(1)

typedef struct allo_vector
{
    double x, y, z;
} allo_vector;

// column major transformation matrix
typedef union allo_m4x4
{
	struct {
		double m11, m12, m13, m14, // 1st column
			m21, m22, m23, m24, // 2nd column, etc
			m31, m32, m33, m34,
			m41, m42, m43, m44;
	};
	double v[16];
} allo_m4x4;

allo_m4x4 allo_m4x4_identity();
allo_m4x4 allo_m4x4_translate(allo_vector translation);
allo_m4x4 allo_m4x4_rotate(double angle, allo_vector axis);
allo_m4x4 allo_m4x4_concat(allo_m4x4 l, allo_m4x4 r);

typedef struct allo_client_pose
{
	allo_m4x4 matrix;
} allo_client_pose;

typedef struct allo_client_poses
{
    allo_client_pose head;
    allo_client_pose left_hand;
    allo_client_pose right_hand;
} allo_client_poses;

typedef struct allo_client_intent
{
    double zmovement; // 1 = maximum speed forwards
    double xmovement; // 1 = maximum speed strafe right
    double yaw;       // rotation around x in radians
    double pitch;     // rotation around y in radians
    allo_client_poses poses;
} allo_client_intent;

typedef struct allo_entity
{
    char *id;

    // exposing implementation detail json isn't _great_ but best I got atm.
    // See https://github.com/alloverse/docs/blob/master/specifications/components.md for official
    // contained values
    cJSON *components;

    LIST_ENTRY(allo_entity) pointers;
} allo_entity;

allo_entity *entity_create(const char *id);
void entity_destroy(allo_entity *entity);

typedef struct allo_state
{
    uint64_t revision;
    LIST_HEAD(allo_entity_list, allo_entity) entities;
} allo_state;

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
void allo_interaction_free(allo_interaction *interaction);

extern bool allo_initialize(bool redirect_stdout);

#pragma pack(pop)
#endif
