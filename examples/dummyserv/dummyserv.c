#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

double gettime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/(double)1e9;
}

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

static void move_stuff_around(alloserver *serv, double delta)
{
    alloserver_client *client = NULL;
    LIST_FOREACH(client, &serv->clients, pointers)
    {
        allo_entity *entity = _clientstate(client)->entity;
        if(!entity) {
            char entity_id[32];
            snprintf(entity_id, 32, "%p", client);
            entity = entity_create(entity_id);
            LIST_INSERT_HEAD(&serv->state.entities, entity, pointers);
        }
        
        entity->position.x += cos(client->intent.pitch)*client->intent.xmovement*delta + sin(client->intent.pitch)*client->intent.zmovement*delta;
        entity->position.z += sin(client->intent.pitch)*client->intent.xmovement*delta + cos(client->intent.pitch)*client->intent.zmovement*delta;
        entity->rotation.x = client->intent.pitch;
        entity->rotation.y = client->intent.yaw;
    }
    // todo: add callback for client disconnect to clean up backref + delete zombie avatar
}

int main(int argc, char **argv) {
    printf("hello world\n");
    alloserver *serv = allo_listen();
    LIST_INIT(&serv->state.entities);

    double time_per_frame = 1/20.0;

    for(;;) {
        double frame_start = gettime();
        for(;;) {
            double elapsed = gettime() - frame_start;
            double remaining = time_per_frame - elapsed;
            if(elapsed > time_per_frame) break;
            serv->interbeat(serv, remaining*1000);
        }
        move_stuff_around(serv, time_per_frame);
        serv->beat(serv);
    }

    return 0;    
}