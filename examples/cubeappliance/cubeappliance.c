#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <cJSON/cJSON.h>
#include "../../src/util.h"
#include <unistd.h>

allo_client_intent intent = {};
char *me;

static void interaction(alloclient *client, const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
    cJSON *cmd = cJSON_Parse(body);
    const char *iname = cJSON_GetStringValue(cJSON_GetArrayItem(cmd, 0));

    if (strcmp(iname, "announce") == 0)
    {
        me = cJSON_GetStringValue(cJSON_GetArrayItem(cmd, 1));
    }
    else if (strcmp(iname, "poke") == 0)
    {
        bool down = cJSON_IsTrue(cJSON_GetArrayItem(cmd, 1));
        if (down)
        {
            intent.xmovement = 1;
            printf("Poked! Moving out of the way...\n");
        }
        else
        {
            intent.xmovement = 0;
            printf("Phew! They stopped poking me, stopping.\n");
        }
        
        client->set_intent(client, intent);
        client->interact(client, "response", me, sender_entity_id, request_id, "[\"poke\", \"ok\"]");
    }
}

int main(int argc, char **argv)
{
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allonet");
        return -1;
    }

    if(argc != 2) {
        fprintf(stderr, "Usage: allodummyclient alloplace://hostname:port\n");
        return -2;
    }

    printf("hello microverse\n");

    cJSON *avatardesco = cjson_create_object(
        "geometry", cjson_create_object(
            "type", cJSON_CreateString("tristrip"),
            "tris", cjson_create_list(
                cJSON_CreateNumber(0.0), cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0),
                cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0),
                cJSON_CreateNumber(-1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0),
                NULL
            ),
            "color", cjson_create_list(
                cJSON_CreateNumber(1.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(0.0), cJSON_CreateNumber(1.0), NULL
            ),
            NULL
        ),
        NULL
    );
    const char *avatardesc = cJSON_Print(avatardesco);

    const char *identity = "{\"display_name\": \"cube\"}";

    alloclient *client = allo_connect(argv[1], identity, avatardesc);
    free((void*)avatardesc);

    if(!client) {
        return -3;
    }
    client->interaction_callback = interaction;

    // step out of the way
    intent.zmovement = 1;
    client->set_intent(client, intent);
    client->poll(client);
    sleep(1);
    intent.zmovement = 0;
    client->set_intent(client, intent);
    
    for(;;)
    {
        client->poll(client);
    }
    return 0;
}
