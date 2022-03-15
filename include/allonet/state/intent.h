#ifndef ALLONET_STATE_INTENT_H
#define ALLONET_STATE_INTENT_H


#include <allonet/math.h>
#include <stdint.h>
#include <stdbool.h>
#include <cJSON/cJSON.h>
#include <allonet/math.h>

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

#ifdef __cplusplus
}
#endif
#endif
