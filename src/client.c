#include <allonet/client.h>
#include <enet/enet.h>
#include <stdio.h>
#include <svc/ntv.h>
#include <string.h>


typedef struct {
    ENetHost *host;
    ENetPeer *peer;
} alloclient_internal;

static alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

static void client_poll(alloclient *client)
{
    ENetEvent event;
    while (enet_host_service(_internal(client)->host, & event, 10) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf ("A new client connected from %x:%u.\n", 
                    event.peer -> address.host,
                    event.peer -> address.port);
            /* Store any relevant client information here. */
            event.peer -> data = "Client information";
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            printf ("A packet of length %u containing %s was received from %s on channel %u.\n",
                    event.packet -> dataLength,
                    event.packet -> data,
                    event.peer -> data,
                    event.channelID);
            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy (event.packet);
            
            break;
        
        case ENET_EVENT_TYPE_DISCONNECT:
            printf ("%s disconnected.\n", event.peer -> data);
            /* Reset the peer's client information. */
            event.peer -> data = NULL;
        }
    }
}

static void client_sendintent(alloclient *client, allo_client_intent intent)
{
    ntv_t *cmdrep = ntv_map(
        "cmd", ntv_str("intent"),
        "intent", ntv_map(
            "zmovement", ntv_double(intent.zmovement),
            "xmovement", ntv_double(intent.xmovement),
            "yaw", ntv_double(intent.yaw),
            "pitch", ntv_double(intent.pitch),
            NULL
        ),
        NULL
    );
    const char *json = ntv_json_serialize_to_str(cmdrep, 0);
    printf("CMD: %s\n", json);
    ntv_release(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength+1] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_COMMANDS, packet);
    free((void*)json);
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
        exit (EXIT_FAILURE);
    }

    ENetAddress address;
    enet_address_set_host (& address, "localhost");
    address.port = 21337;

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
        puts ("Connection to some.server.net:1234 succeeded.");
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
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->host = host;
    _internal(client)->peer = peer;
    //enet_host_destroy(client);

    return client;
}