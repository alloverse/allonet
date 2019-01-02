#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H
#include "inlinesys/queue.h"
#include <stdint.h>
#include <stdbool.h>

#pragma pack(push)
#pragma pack(1)

typedef struct {
    double zmovement; // 1 = maximum speed forwards
    double xmovement; // 1 = maximum speed strafe right
    double yaw; // rotation around x in radians
    double pitch; // rotation around y in radians
} allo_client_intent;

typedef struct {
    double x, y, z;
} allo_vector;

typedef struct allo_entity {
    char *id;
    allo_vector position;
    allo_vector rotation; // radians

    LIST_ENTRY(allo_entity) pointers;
} allo_entity;

allo_entity *entity_create(const char *id);
void entity_destroy(allo_entity *entity);

typedef struct {
    uint64_t revision;
    LIST_HEAD(allo_entity_list, allo_entity) entities;
} allo_state;

extern bool allo_initialize(bool redirect_stdout);

#pragma pack(pop)
#endif
