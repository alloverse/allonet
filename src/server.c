#include <allonet/server.h>
#include <enet/enet.h>
#include <stdio.h>

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
        serv->clients[serv->client_count++] = new_client;
        if(serv->clients_callback) {
            serv->clients_callback(serv, serv->clients, serv->client_count);
        }
        break; }
    
    case ENET_EVENT_TYPE_RECEIVE: {
        printf ("A packet of length %zu containing %s was received from %s on channel %u.\n",
                event.packet -> dataLength,
                event.packet -> data,
                event.peer -> data,
                event.channelID);
        /* Clean up the packet now that we're done using it. */
        enet_packet_destroy (event.packet);
        
        break; }
    
    case ENET_EVENT_TYPE_DISCONNECT: {
        printf ("%s disconnected.\n", event.peer -> data);
        for(int i = 0; i < allo_client_count_max; i++) {
            if(serv->clients[i] == event.peer->data) {
                alloserv_client_free(serv->clients[i]);
                for(int j = i; j < allo_client_count_max-1; j++) {
                    serv->clients[j] = serv->clients[j+1];
                }
                serv->clients[allo_client_count_max-1] = NULL;
                break;
            }
        }
        event.peer->data = NULL;
        if(serv->clients_callback) {
            serv->clients_callback(serv, serv->clients, serv->client_count);
        }
        break; }
    case ENET_EVENT_TYPE_NONE:break;
    }
}

static void allo_sendstates(alloserver *serv)
{

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
        2,  // 2 channels
        0,  // no ingress bandwidth limit      /* assume any amount of incoming bandwidth */,
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
    
    return serv;
}
