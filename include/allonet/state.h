#ifndef ALLONET_STATE_H
#define ALLONET_STATE_H

typedef struct {
    double zmovement; // 1 = maximum speed forwards
    double xmovement; // 1 = maximum speed strafe right
    double yaw;
    double pitch;
} allo_client_intent;

typedef struct {
    double x, y, z;
} allo_vector;

typedef struct {
    const char *id;
    allo_vector position;
    allo_vector rotation;
} allo_entity;

typedef struct {
    long long revision;
    allo_entity *entities;
    int entity_count;
} allo_state;

#endif
