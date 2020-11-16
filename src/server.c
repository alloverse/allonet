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

static void handle_incoming_connection(alloserver *serv, ENetPeer* new_peer)
{
    alloserver_client *new_client = _client_create();
    char host[255] = {0};
    enet_address_get_host_ip(&new_peer->address, host, 254);
    printf ("A new client connected from %s:%u as %s/%p.\n", 
        host,
        new_peer->address.port,
        new_client->agent_id,
        new_client
    );
    
    _clientinternal(new_client)->peer = new_peer;
    _clientinternal(new_client)->peer->data = (void*)new_client;
    LIST_INSERT_HEAD(&serv->clients, new_client, pointers);

    // very hard timeout limits; change once clients actually send SYN
    enet_peer_timeout(new_peer, 0, 5000, 5000);
    if(serv->clients_callback) {
        serv->clients_callback(serv, new_client, NULL);
    }
}

static void handle_incoming_data(alloserver *serv, alloserver_client *client, allochannel channel, ENetPacket *packet)
{
    // todo: stop newline terminating in protocol...
    packet->data[packet->dataLength-1] = 0;
    
    if(channel == CHANNEL_MEDIA)
    {
        alloserver_client *other;
        LIST_FOREACH(other, &serv->clients, pointers)
        {
            if(other == client) continue;

            allo_send(serv, other, CHANNEL_MEDIA, packet->data, packet->dataLength);
        }
        return;
    }
    
    if(serv->raw_indata_callback)
    {
        serv->raw_indata_callback(
            serv, 
            client, 
            channel, 
            packet->data,
            packet->dataLength
        );
    }
}

static void handle_lost_connection(alloserver *serv, alloserver_client *client)
{
    char host[255] = {0};
    ENetPeer *peer = _clientinternal(client)->peer;
    enet_address_get_host_ip(&peer->address, host, 254);
    printf("%s/%p from %s:%d disconnected.\n", client->agent_id, client, host, peer->address.port);

    LIST_REMOVE(client, pointers);
    peer->data = NULL;
    if(serv->clients_callback) {
        serv->clients_callback(serv, NULL, client);
    }
    alloserv_client_free(client);
}

static bool allo_poll(alloserver *serv, int timeout)
{
    ENetEvent event;
    enet_host_service (_servinternal(serv)->enet, &event, timeout);
    alloserver_client *client = event.peer ? (alloserver_client*)event.peer->data : NULL;

    switch (event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            handle_incoming_connection(serv, event.peer);
            break;
    
        case ENET_EVENT_TYPE_RECEIVE:
            if (client == NULL) {
                // old data from disconnected client?!
                break;
            }
            handle_incoming_data(serv, client, event.channelID, event.packet);
            enet_packet_destroy (event.packet);
            break;
    
        case ENET_EVENT_TYPE_DISCONNECT:
            handle_lost_connection(serv, client);
            break;

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

alloserver *allo_listen(int listenhost, int port)
{
    alloserver *serv = (alloserver*)calloc(1, sizeof(alloserver));
    serv->_internal = (alloserv_internal*)calloc(1, sizeof(alloserv_internal));
    srand((unsigned int)time(NULL));

    ENetAddress address;
    address.host = listenhost;
    address.port = port;
    char printable[255] = {0};
    enet_address_get_host_ip(&address, printable, 254);
    printf("Alloserv attempting listen on %s:%d...\n", printable, port);
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
        alloserv_stop(serv);
        return NULL;
    }

    serv->_port = _servinternal(serv)->enet->address.port;
    serv->interbeat = allo_poll;
    serv->send = allo_send;
    LIST_INIT(&serv->clients);
    LIST_INIT(&serv->state.entities);
    
    return serv;
}

void alloserv_disconnect(alloserver *serv, alloserver_client *client, int reason_code)
{
    enet_peer_disconnect_later(_clientinternal(client)->peer, reason_code);
}

void alloserv_stop(alloserver* serv)
{
  enet_host_destroy(_servinternal(serv)->enet);
  free(_servinternal(serv));
  free(serv);
}

int allo_socket_for_select(alloserver *serv)
{
    return _servinternal(serv)->enet->socket;
}
