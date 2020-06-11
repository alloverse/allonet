#include <allonet/server.h>
#include <enet/enet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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
    for(int i = 0; i < AGENT_ID_LENGTH; i++)
    {
        client->agent_id[i] = 'a' + rand() % 25;
    }
    client->agent_id[AGENT_ID_LENGTH] = '\0';
    client->intent = allo_client_intent_create();

    return client;
}

static void alloserv_client_free(alloserver_client *client)
{
    allo_client_intent_free(client->intent);
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
        alloserver_client *new_client = _client_create();
        printf ("A new client connected from %x:%u as %s.\n", 
            event.peer -> address.host,
            event.peer -> address.port,
            new_client->agent_id
        );
        
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
        if (client == NULL) {
          // old data from disconnected client?!
          break;
        }
        
        // todo: stop newline terminating in protocol...
        event.packet->data[event.packet->dataLength-1] = 0;
        
        if(serv->raw_indata_callback)
        {
            serv->raw_indata_callback(
                serv, 
                client, 
                event.channelID, 
                event.packet->data,
                event.packet->dataLength
            );
        }
        
        enet_packet_destroy (event.packet);
        
        break;
    }
    
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


void allo_send(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *buf, int len)
{
    ENetPacket *packet = enet_packet_create(
        NULL,
        len,
        (channel==CHANNEL_COMMANDS || channel==CHANNEL_ASSETS) ?
            ENET_PACKET_FLAG_RELIABLE :
            0
    );
    memcpy(packet->data, buf, len);
    packet->data[len-1] = '\n';
    enet_peer_send(_clientinternal(client)->peer, channel, packet);
}

alloserver *allo_listen(int port)
{
    alloserver *serv = (alloserver*)calloc(1, sizeof(alloserver));
    serv->_internal = (alloserv_internal*)calloc(1, sizeof(alloserv_internal));
    srand((unsigned int)time(NULL));

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

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
    serv->send = allo_send;
    LIST_INIT(&serv->clients);
    LIST_INIT(&serv->state.entities);
    
    return serv;
}

int allo_socket_for_select(alloserver *serv)
{
    return _servinternal(serv)->enet->socket;
}
