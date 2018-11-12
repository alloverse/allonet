#include <allonet/client.h>
#include <enet/enet.h>
#include <stdio.h>
#include <svc/ntv.h>
#include <string.h>

alloclient *allo_connect(const char *url)
{
    ENetHost * client;
    client = enet_host_create (NULL /* create a client host */,
            1 /* only allow 1 outgoing connection */,
            2 /* allow up 2 channels to be used, 0 and 1 */,
            0 /* assume any amount of incoming bandwidth */,
            0 /* assume any amount of incoming bandwidth */);
    if (client == NULL)
    {
        fprintf (stderr, 
                "An error occurred while trying to create an ENet client host.\n");
        exit (EXIT_FAILURE);
    }

    ENetAddress address;
    enet_address_set_host (& address, "localhost");
    address.port = 21337;

    ENetPeer *peer;
    peer = enet_host_connect (client, & address, 2, 0);    
    if (peer == NULL)
    {
        fprintf (stderr, 
                    "No available peers for initiating an ENet connection.\n");
        exit (EXIT_FAILURE);
    }


    ENetEvent event;

    if (enet_host_service (client, & event, 5000) > 0 &&
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
        puts ("Connection to some.server.net:1234 failed.");
        exit(EXIT_FAILURE);
    }


    for(;;)
    {
        while (enet_host_service (client, & event, 1000) > 0)
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


    enet_host_destroy(client);

    return NULL;
}