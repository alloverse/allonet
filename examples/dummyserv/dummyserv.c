#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

typedef struct dummy_clientstate
{
    allo_entity *entity;
} dummy_clientstate;

static dummy_clientstate *_clientstate(alloserver_client *client)
{
    dummy_clientstate *state = (dummy_clientstate*)client->_backref;
    if(!state) {
        client->_backref = state = (dummy_clientstate *)calloc(1, sizeof(dummy_clientstate));
    }
    return state;
}

// from private util :P
extern int64_t get_ts_mono(void);

static void move_stuff_around(alloserver *serv, double delta)
{
    alloserver_client *client = NULL;
    LIST_FOREACH(client, &serv->clients, pointers)
    {
        allo_entity *entity = _clientstate(client)->entity;
        entity->position.x += cos(client->intent.pitch)*client->intent.xmovement*delta + sin(client->intent.pitch)*client->intent.zmovement*delta;
        entity->position.z += sin(client->intent.pitch)*client->intent.xmovement*delta + cos(client->intent.pitch)*client->intent.zmovement*delta;
        entity->rotation.x = client->intent.pitch;
        entity->rotation.y = client->intent.yaw;
    }
}

static void clients_changed(alloserver *serv, alloserver_client *added, alloserver_client *removed)
{
    if(added)
    {
        char entity_id[32];
        snprintf(entity_id, 32, "%p", added);
        allo_entity *entity = entity_create(entity_id);
        LIST_INSERT_HEAD(&serv->state.entities, entity, pointers);
        _clientstate(added)->entity = entity;
        char *cmd = malloc(strlen(entity_id)+100);
        sprintf(cmd, "[\"your_avatar\", \"%s\"]", entity_id);
        serv->interact(serv, added, NULL, NULL, cmd);
        free(cmd);
    } 
    else if(removed)
    {
        allo_entity *entity = _clientstate(removed)->entity;
        LIST_REMOVE(entity, pointers);
        free(_clientstate(removed));
    }
}

int main(int argc, char **argv) {
    printf("hello world\n");
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allonet");
        return -1;
    }

    alloserver *serv = allo_listen();
    serv->clients_callback = clients_changed;
    LIST_INIT(&serv->state.entities);

    double time_per_frame = 1/20.0;

    for(;;) {
        double frame_start = get_ts_mono();
        for(;;) {
            double elapsed = get_ts_mono() - frame_start;
            double remaining = time_per_frame - elapsed;
            if(elapsed > time_per_frame) break;
            serv->interbeat(serv, remaining*1000);
        }
        move_stuff_around(serv, time_per_frame);
        serv->beat(serv);
    }

    return 0;    
}
