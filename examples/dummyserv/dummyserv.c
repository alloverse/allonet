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
allo_vector cjson2vec(const cJSON *veclist);
cJSON *vec2cjson(allo_vector vec);
cJSON *cjson_create_object(const char *key, cJSON *value, ...);

static void move_stuff_around(alloserver *serv, double delta)
{
    alloserver_client *client = NULL;
    LIST_FOREACH(client, &serv->clients, pointers)
    {
        allo_entity *entity = _clientstate(client)->entity;
        cJSON *transform = cJSON_GetObjectItem(entity->components, "transform");
        allo_vector position = cjson2vec(cJSON_GetObjectItem(transform, "position"));
        allo_vector rotation = cjson2vec(cJSON_GetObjectItem(transform, "rotation"));
        
        position.x += cos(client->intent.pitch)*client->intent.xmovement*delta + sin(client->intent.pitch)*client->intent.zmovement*delta;
        position.z += sin(client->intent.pitch)*client->intent.xmovement*delta + cos(client->intent.pitch)*client->intent.zmovement*delta;
        rotation.x = client->intent.pitch;
        rotation.y = client->intent.yaw;
        
        cJSON_ReplaceItemInObject(transform, "position", vec2cjson(position));
        cJSON_ReplaceItemInObject(transform, "rotation", vec2cjson(rotation));
    }
}

static void clients_changed(alloserver *serv, alloserver_client *added, alloserver_client *removed)
{
    if(added)
    {
        char entity_id[32];
        snprintf(entity_id, 32, "%p", added);
        allo_entity *entity = entity_create(entity_id);
        allo_vector zero;
        memset(&zero, 0, sizeof(zero));
        entity->components = cjson_create_object(
            "transform", cjson_create_object(
                "position", vec2cjson(zero),
                "rotation", vec2cjson(zero),
                NULL
            ),
            NULL
        );
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
