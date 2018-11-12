#include <allonet/server.h>
#include <enet/enet.h>
#include <stdio.h>
#include <svc/ntv.h>
#include <string.h>

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

static void allo_poll(alloserver *serv, int timeout)
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
        if(serv->clients_callback) {
            serv->clients_callback(serv, new_client, NULL);
        }
        break; }
    
    case ENET_EVENT_TYPE_RECEIVE: {
        alloserver_client *client = (alloserver_client*)event.peer->data;
        
        event.packet->data[event.packet->dataLength-1] = 0;
        ntv_t *cmd = ntv_json_deserialize((char*)(event.packet->data), NULL, 0);
        const char *cmdname = ntv_get_str(cmd, "cmd");
        printf("Client %p sent command %s\n", client, cmdname);
        if(strcmp(cmdname, "intent") == 0) {
            const ntv_t *ntvintent = ntv_get_map(cmd, "intent");
            allo_client_intent intent = {
                .zmovement = ntv_get_double(ntvintent, "zmovement", 0),
                .xmovement = ntv_get_double(ntvintent, "xmovement", 0),
                .yaw = ntv_get_double(ntvintent, "yaw", 0),
                .pitch = ntv_get_double(ntvintent, "pitch", 0),
            };
            client->intent = intent;
        }

        enet_packet_destroy (event.packet);
        
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
    case ENET_EVENT_TYPE_NONE:break;
    }
}

static void allo_sendstates(alloserver *serv)
{
    ntv_t *entities_rep = ntv_create_list();
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &serv->state.entities, pointers) {
        ntv_t *entity_rep = ntv_map(
            "id", ntv_str(entity->id),
            "position", ntv_list(ntv_double(entity->position.x), ntv_double(entity->position.y), ntv_double(entity->position.z), NULL),
            "rotation", ntv_list(ntv_double(entity->rotation.x), ntv_double(entity->rotation.y), ntv_double(entity->rotation.z), NULL),
            NULL
        );
        entity_rep->ntv_parent = entities_rep;
        TAILQ_INSERT_TAIL(&entities_rep->ntv_children, entity_rep, ntv_link);
    }
    ntv_t *map = ntv_map(
        "entities", entities_rep, 
        "revision", ntv_int(serv->state.revision++),
        NULL
    );
    const char *json = ntv_json_serialize_to_str(map, 0);
    printf("STATE: %s\n", json);
    ntv_release(map);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_UNSEQUENCED);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength+1] = '\n';
    free((void*)json);
    alloserver_client *client;
    LIST_FOREACH(client, &serv->clients, pointers) {
        alloserv_client_internal *internal = _clientinternal(client);
        enet_peer_send(internal->peer, CHANNEL_STATEDIFFS, packet);
    }
}

alloserver *allo_listen(void)
{
    alloserver *serv = (alloserver*)calloc(1, sizeof(alloserver));
    serv->_internal = (alloserv_internal*)calloc(1, sizeof(alloserv_internal));
    if (enet_initialize () != 0)
    {
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        allo_free(serv);
        return NULL;
    }
    atexit (enet_deinitialize);

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
    LIST_INIT(&serv->clients);
    
    return serv;
}
