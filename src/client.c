#include <allonet/client.h>
#include <enet/enet.h>
#include <stdio.h>
#include <svc/ntv.h>
#include <string.h>
#include <svc/strvec.h>

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

static allo_vector ntv2vec(const ntv_t *x)
{
    const ntv_t *y = x->ntv_children.tqh_first;
    const ntv_t *z = y->ntv_link.tqe_next;
    return (allo_vector){x->ntv_double, y->ntv_double, z->ntv_double};
}

static void parse_statediff(alloclient *client, ENetPacket *packet)
{
    packet->data[packet->dataLength-1] = 0;
    ntv_t *cmd = ntv_json_deserialize((char*)(packet->data), NULL, 0);
    int64_t rev = ntv_get_int64(cmd, "revision", 0);
    const ntv_t *entities = ntv_get_list(cmd, "entities");
    client->state.revision = rev;
    
    // keep track of entities that aren't mentioned in the incoming list
    scoped_strvec(deletes);
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &client->state.entities, pointers)
    {
        strvec_insert_sorted(&deletes, entity->id);
    }
    
    // update or create entities
    NTV_FOREACH(edesc, entities) {
        const char *entity_id = ntv_get_str(edesc, "id");
        strvec_delete_value(&deletes, entity_id);
        const ntv_t *position = ntv_get_list(edesc, "position");
        const ntv_t *rotation = ntv_get_list(edesc, "rotation");
        allo_entity *entity = get_entity(client, entity_id);
        if(!entity) {
            entity = entity_create(entity_id);
            printf("Creating entity %s\n", entity->id);
            LIST_INSERT_HEAD(&client->state.entities, entity, pointers);
        }
        entity->position = ntv2vec(position);
        entity->rotation = ntv2vec(rotation);
    }

    // now, delete old entities
    entity = client->state.entities.lh_first;
    while(entity)
    {
        allo_entity *to_delete = entity;
        entity = entity->pointers.le_next;
        if(strvec_find(&deletes, to_delete->id) == 0) {
            printf("Deleting entity %s\n", to_delete->id);
            LIST_REMOVE(to_delete, pointers);
            entity_destroy(to_delete);
        }
    }
}

static void parse_packet_from_channel(alloclient *client, ENetPacket *packet, allochannel channel)
{
    switch(channel) {
    case CHANNEL_STATEDIFFS:
        parse_statediff(client, packet);
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
    LIST_INIT(&client->state.entities);
    

    return client;
}

// todo: disconnect & teardown
//enet_host_destroy(client);
