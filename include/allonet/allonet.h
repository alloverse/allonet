#ifndef ALLONET_H
#define ALLONET_H

typedef enum {
    allo_unreliable = 1,
    allo_reliable = 2,
} allo_sendmode;

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
    allo_entity *entities;
    int entity_count;
} allo_state;

typedef struct {
    // call this to change this client's movement/action intent
    void (*set_intent)(allo_client_intent intent);
    // rpc on another entity
    void (*interact)(const char *entity_id, const char *cmd);
    void (*disconnect)(void);
    
    // set this to get a callback when state changes. data in 'state'
    // is valid only during duration of callback.
    void (*state_callback)(allo_state *state);


    // internal
    void (*send)(allo_sendmode mode, const char *buf, int len);
    allo_state state;
} alloclient;

alloclient *allo_connect(const char *url);



#endif
