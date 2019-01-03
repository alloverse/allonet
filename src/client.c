#include <allonet/client.h>
#include <enet/enet.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include "util.h"

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
} alloclient_internal;

static alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

static allo_entity *get_entity(alloclient *client, const char *entity_id)
{
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &client->state.entities, pointers)
    {
        if(strcmp(entity_id, entity->id) == 0) 
        {
            return entity;
        }
    }
    return NULL;
}

static void parse_statediff(alloclient *client, ENetPacket *packet)
{
    packet->data[packet->dataLength-1] = 0;
    cJSON *cmd = cJSON_Parse((const char*)(packet->data));
    int64_t rev = cJSON_GetObjectItem(cmd, "revision")->valueint;
    const cJSON *entities = cJSON_GetObjectItem(cmd, "entities");
    client->state.revision = rev;
    
    // keep track of entities that aren't mentioned in the incoming list
    cJSON *deletes = cJSON_CreateArray();
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &client->state.entities, pointers)
    {
        cJSON_AddItemToArray(deletes, cJSON_CreateString(entity->id));
    }
    
    // update or create entities
    const cJSON *edesc = NULL;
    cJSON_ArrayForEach(edesc, entities) {
        const char *entity_id = cJSON_GetObjectItem(edesc, "id")->valuestring;
        cjson_delete_from_array(deletes, entity_id);
        const cJSON *position = cJSON_GetObjectItem(edesc, "position");
        const cJSON *rotation = cJSON_GetObjectItem(edesc, "rotation");
        allo_entity *entity = get_entity(client, entity_id);
        if(!entity) {
            entity = entity_create(entity_id);
            printf("Creating entity %s\n", entity->id);
            LIST_INSERT_HEAD(&client->state.entities, entity, pointers);
        }
        entity->position = cjson2vec(position);
        entity->rotation = cjson2vec(rotation);
    }

    // now, delete old entities
    entity = client->state.entities.lh_first;
    while(entity)
    {
        allo_entity *to_delete = entity;
        entity = entity->pointers.le_next;
        if(cjson_find_in_array(deletes, to_delete->id) != -1) {
            printf("Deleting entity %s\n", to_delete->id);
            LIST_REMOVE(to_delete, pointers);
            entity_destroy(to_delete);
        }
    }
    cJSON_free(cmd);
    cJSON_free(deletes);
}

static void parse_interaction(alloclient *client, cJSON *cmdrep)
{
    const cJSON *interaction = cJSON_GetObjectItem(cmdrep, "interact");
    const char *from = cJSON_GetObjectItem(interaction, "from_entity")->valuestring;
    const char *to = cJSON_GetObjectItem(interaction, "to_entity")->valuestring;
    const char *cmd = cJSON_GetObjectItem(interaction, "cmd")->valuestring;
    if(client->interaction_callback) {
        client->interaction_callback(client, from, to, cmd);
    }
}

static void parse_command(alloclient *client, cJSON *cmdrep)
{
    const char *cmdname = cJSON_GetObjectItem(cmdrep, "cmd")->valuestring;
    if(strcmp(cmdname, "interact") == 0) {
        parse_interaction(client, cmdrep);
    } else {
        printf("Unknown command: %s\n", cmdrep->string);
    }
}

static void parse_packet_from_channel(alloclient *client, ENetPacket *packet, allochannel channel)
{
    switch(channel) {
    case CHANNEL_STATEDIFFS:
        parse_statediff(client, packet);
        break;
    case CHANNEL_COMMANDS: {
        cJSON *cmdrep = cJSON_Parse((const char*)(packet->data));
        parse_command(client, cmdrep);
        cJSON_free(cmdrep);
        break; }
    default: break;
    }
}

static void client_poll(alloclient *client)
{
    ENetEvent event;
    while (enet_host_service(_internal(client)->host, & event, 10) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            parse_packet_from_channel(client, event.packet, (allochannel)event.channelID);
            enet_packet_destroy (event.packet);
            break;
        
        case ENET_EVENT_TYPE_DISCONNECT:
            printf ("Disconnected.\n");
        default: break;
        }
    }
}

static void client_sendintent(alloclient *client, allo_client_intent intent)
{
    cJSON *cmdrep = cjson_create_object(
        "cmd", cJSON_CreateString("intent"),
        "intent", cjson_create_object(
            "zmovement", cJSON_CreateNumber(intent.zmovement),
            "xmovement", cJSON_CreateNumber(intent.xmovement),
            "yaw", cJSON_CreateNumber(intent.yaw),
            "pitch", cJSON_CreateNumber(intent.pitch),
            NULL
        ),
        NULL
    );
    const char *json = cJSON_Print(cmdrep);
    cJSON_free(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength+1] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_COMMANDS, packet);
    free((void*)json);
}

static void client_disconnect(alloclient *client, int reason)
{
    enet_peer_disconnect(_internal(client)->peer, reason);
    int now = get_ts_mono();
    int started_at = now;
    int end_at = started_at + 1000;
    ENetEvent event;
    memset(&event, 0, sizeof(event));
    while(now < end_at) {
        enet_host_service(_internal(client)->host, & event, end_at-now);
        if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
            puts("Successfully disconnected");
            break;
        }
        now = get_ts_mono();
    }
    if(event.type != ENET_EVENT_TYPE_DISCONNECT) {
        puts("Disconnection timed out");
    }
    enet_host_destroy(_internal(client)->host);
    free(_internal(client));
    free(client);
}

alloclient *allo_connect(const char *url)
{
    ENetHost * host;
    host = enet_host_create (NULL /* create a client host */,
            1 /* only allow 1 outgoing connection */,
            2 /* allow up 2 channels to be used, 0 and 1 */,
            0 /* assume any amount of incoming bandwidth */,
            0 /* assume any amount of incoming bandwidth */);
    if (host == NULL)
    {
        fprintf (stderr, 
                "An error occurred while trying to create an ENet client host.\n");
        return NULL;
    }

    // parse the url in the form of alloplace://hostname{:port}{/}
    if (strstr(url, "alloplace://") == 0) {
        fprintf(stderr, "Not an alloplace URL\n");
        return NULL;
    }
    const char *hostandport = strstr(url, "://")+3;
    const char *port = strstr(hostandport, ":"); if(port) port+=1;
    const char *slash = strstr(hostandport, "/");
    char *justhost = allo_strndup(hostandport,
        port ? // if host part contains port descriptor...
            (int)(port-hostandport-1) : // dup up until and excluding the colon
            slash ? // otherwise, if there's a slash...
                (int)(slash-hostandport) : // dup up until and excluding the slash
                strlen(hostandport) // otherwise, dup the whole string
    );
    char *justport = (port && slash) ? allo_strndup(port, slash-port) : port ? strdup(port) : NULL;
    
    ENetAddress address;
    enet_address_set_host (& address, justhost);
    address.port = justport ? atoi(justport) : 21337;

    printf("Connecting to %s:%d\n", justhost, address.port);
    free(justhost);
    free(justport);

    ENetPeer *peer;
    peer = enet_host_connect (host, & address, 2, 0);    
    if (peer == NULL)
    {
        fprintf (stderr, 
                    "No available peers for initiating an ENet connection.\n");
        return NULL;
    }


    ENetEvent event;
    if (enet_host_service(host, & event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        puts ("Connection succeeded.");
    }
    else
    {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset (peer);
        puts ("Connection to server failed.");
        return NULL;
    }

    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->poll = client_poll;
    client->set_intent = client_sendintent;
    client->disconnect = client_disconnect;
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->host = host;
    _internal(client)->peer = peer;
    LIST_INIT(&client->state.entities);
    

    return client;
}

