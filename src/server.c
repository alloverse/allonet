#include <allonet/server.h>
#include <enet/enet.h>
#include <stdio.h>
#include <string.h>
#include <cJSON/cJSON.h>
#include "util.h"

typedef struct {
    ENetHost *enet;
} alloserv_internal;

typedef struct {
    ENetPeer *peer;
} alloserv_client_internal;

static alloserv_internal *_servinternal(alloserver *serv)
{
    return (alloserv_internal*)serv->_internal;
}

static alloserv_client_internal *_clientinternal(alloserver_client *client)
{
    return (alloserv_client_internal*)client->_internal;
}

static alloserver_client *_client_create()
{
    alloserver_client *client = (alloserver_client*)calloc(1, sizeof(alloserver_client));
    client->_internal = (void*)calloc(1, sizeof(alloserv_client_internal));
    return client;
}

static void alloserv_client_free(alloserver_client *client)
{
    free(_clientinternal(client));
    free(client);
}

static void allo_free(alloserver *serv)
{
    enet_host_destroy(_servinternal(serv)->enet);
    free(_servinternal(serv));
    free(serv);
}

static bool allo_poll(alloserver *serv, int timeout)
{
    ENetEvent event;
    enet_host_service (_servinternal(serv)->enet, &event, timeout);
    
    switch (event.type)
    {
    case ENET_EVENT_TYPE_CONNECT: {
        printf ("A new client connected from %x:%u.\n", 
                event.peer -> address.host,
                event.peer -> address.port);
        alloserver_client *new_client = _client_create();
        _clientinternal(new_client)->peer = event.peer;
        _clientinternal(new_client)->peer->data = (void*)new_client;
        LIST_INSERT_HEAD(&serv->clients, new_client, pointers);

        // very hard timeout limits; change once clients actually send SYN
        enet_peer_timeout(event.peer, 0, 5000, 5000);
        if(serv->clients_callback) {
            serv->clients_callback(serv, new_client, NULL);
        }
        break; }
    
    case ENET_EVENT_TYPE_RECEIVE: {
        alloserver_client *client = (alloserver_client*)event.peer->data;
        
        // todo: stop newline terminating in protocol...
        event.packet->data[event.packet->dataLength-1] = 0;
        
        if(serv->raw_indata_callback)
            serv->raw_indata_callback(serv, client, event.channelID, event.packet->data, event.packet->dataLength-1);
        
        // todo: change to ["intent", intentpayload]
        cJSON *cmd = cJSON_Parse((char*)(event.packet->data));
        const char *cmdname = cJSON_GetObjectItem(cmd, "cmd")->valuestring;
        if(strcmp(cmdname, "intent") == 0) {
            const cJSON *ntvintent = cJSON_GetObjectItem(cmd, "intent");
            allo_client_intent intent = {
                .zmovement = cJSON_GetObjectItem(ntvintent, "zmovement")->valuedouble,
                .xmovement = cJSON_GetObjectItem(ntvintent, "xmovement")->valuedouble,
                .yaw = cJSON_GetObjectItem(ntvintent, "yaw")->valuedouble,
                .pitch = cJSON_GetObjectItem(ntvintent, "pitch")->valuedouble,
            };
            client->intent = intent;
            if(serv->intent_callback)
                serv->intent_callback(serv, client);
        } else {
            printf("Client %p sent unknown command %s\n", client, cmdname);
        }

        enet_packet_destroy (event.packet);
        cJSON_Delete(cmd);
        
        break; }
    
    case ENET_EVENT_TYPE_DISCONNECT: {
        alloserver_client *client = (alloserver_client*)event.peer->data;
        printf("%p disconnected.\n", client);
        LIST_REMOVE(client, pointers);
        event.peer->data = NULL;
        if(serv->clients_callback) {
            serv->clients_callback(serv, NULL, client);
        }
        alloserv_client_free(client);
        break; }
    case ENET_EVENT_TYPE_NONE:
        return false;
    }
    return true;
}

static void allo_sendstates(alloserver *serv)
{
    cJSON *entities_rep = cJSON_CreateArray();
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &serv->state.entities, pointers) {
        cJSON *entity_rep = cjson_create_object(
            "id", cJSON_CreateString(entity->id),
            NULL
        );
        cJSON_AddItemReferenceToObject(entity_rep, "components", entity->components);
        cJSON_AddItemToArray(entities_rep, entity_rep);
    }
    cJSON *map = cjson_create_object(
        "entities", entities_rep, 
        "revision", cJSON_CreateNumber(serv->state.revision++),
        NULL
    );
    const char *json = cJSON_Print(map);
    printf("STATE: %s\n", json);
    cJSON_Delete(map);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_UNSEQUENCED);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength] = '\n';
    free((void*)json);
    alloserver_client *client;
    LIST_FOREACH(client, &serv->clients, pointers) {
        alloserv_client_internal *internal = _clientinternal(client);
        enet_peer_send(internal->peer, CHANNEL_STATEDIFFS, packet);
    }
}

void server_interact(alloserver *serv, alloserver_client *client, const char *from_entity, const char *to_entity, const char *cmd)
{
    cJSON *cmdrep = cjson_create_object(
        "cmd", cJSON_CreateString("interact"),
        "interact", cjson_create_object(
            "from_entity", cJSON_CreateString(from_entity ? from_entity : ""),
            "to_entity", cJSON_CreateString(to_entity ? to_entity : ""),
            "cmd", cJSON_CreateString(cmd),
            NULL
        ),
        NULL
    );
    const char *json = cJSON_Print(cmdrep);
    cJSON_Delete(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength+1] = '\n';
    enet_peer_send(_clientinternal(client)->peer, CHANNEL_COMMANDS, packet);
    free((void*)json);
}

void allo_send(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *buf, int len)
{
    ENetPacket *packet = enet_packet_create(
        NULL,
        len,
        channel==CHANNEL_COMMANDS ?
            ENET_PACKET_FLAG_RELIABLE :
            0
    );
    memcpy(packet->data, buf, len);
    enet_peer_send(_clientinternal(client)->peer, channel, packet);
}

alloserver *allo_listen(void)
{
    alloserver *serv = (alloserver*)calloc(1, sizeof(alloserver));
    serv->_internal = (alloserv_internal*)calloc(1, sizeof(alloserv_internal));
    

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = allo_udp_port;

    _servinternal(serv)->enet = enet_host_create(
        &address,
        allo_client_count_max,
        CHANNEL_COUNT,
        0,  // no ingress bandwidth limit
        0   // no egress bandwidth limit
    );
    if (_servinternal(serv)->enet == NULL)
    {
        fprintf (
            stderr, 
             "An error occurred while trying to create an ENet server host.\n"
        );
        allo_free(serv);
        return NULL;
    }

    serv->interbeat = allo_poll;
    serv->beat = allo_sendstates;
    serv->interact = server_interact;
    serv->send = allo_send;
    LIST_INIT(&serv->clients);
    LIST_INIT(&serv->state.entities);
    
    return serv;
}

int allo_socket_for_select(alloserver *serv)
{
    return _servinternal(serv)->enet->socket;
}
