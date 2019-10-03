#include <allonet/client.h>
#include <enet/enet.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "util.h"

#if !defined(NDEBUG) && (defined(__clang__) || defined(__GNUC__))
    #define nonnull(x) ({ typeof(x) xx = (x); assert(xx != NULL); xx; })
#else
    #define nonnull(x) x
#endif

typedef struct interaction_queue {
    allo_interaction *interaction;
    LIST_ENTRY(interaction_queue) pointers;
} interaction_queue;

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
    LIST_HEAD(interaction_queue_list, interaction_queue) interactions;
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

static void parse_statediff(alloclient *client, cJSON *cmd)
{
    int64_t rev = nonnull(cJSON_GetObjectItem(cmd, "revision"))->valueint;
    const cJSON *entities = nonnull(cJSON_GetObjectItem(cmd, "entities"));
    client->state.revision = rev;
    
    // keep track of entities that aren't mentioned in the incoming list
    cJSON *deletes = cJSON_CreateArray();
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &client->state.entities, pointers)
    {
        cJSON_AddItemToArray(deletes, cJSON_CreateString(entity->id));
    }
    
    // update or create entities
    cJSON *edesc = NULL;
    cJSON_ArrayForEach(edesc, entities) {
        const char *entity_id = nonnull(cJSON_GetObjectItem(edesc, "id"))->valuestring;
        cjson_delete_from_array(deletes, entity_id);
        cJSON *components = nonnull(cJSON_DetachItemFromObject(edesc, "components"));
        allo_entity *entity = get_entity(client, entity_id);
        if(!entity) {
            entity = entity_create(entity_id);
            printf("Creating entity %s\n", entity->id);
            LIST_INSERT_HEAD(&client->state.entities, entity, pointers);
        }
        
        cJSON_Delete(entity->components);
        entity->components = components;
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
    cJSON_Delete(deletes);
}

static void parse_interaction(alloclient *client, cJSON *interaction)
{
    const char *type = nonnull(cJSON_GetArrayItem(interaction, 1))->valuestring;
    const char *from = nonnull(cJSON_GetArrayItem(interaction, 2))->valuestring;
    const char *to = nonnull(cJSON_GetArrayItem(interaction, 3))->valuestring;
    const char *request_id = nonnull(cJSON_GetArrayItem(interaction, 4))->valuestring;
    cJSON *body = nonnull(cJSON_GetArrayItem(interaction, 5));
    const char *bodystr = cJSON_Print(body);
    if(client->interaction_callback) {
        client->interaction_callback(client, type, from, to, request_id, bodystr);
    } else {
        allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
        interaction_queue *entry = (interaction_queue*)calloc(1, sizeof(interaction_queue));
        entry->interaction = interaction;
        LIST_INSERT_HEAD(&_internal(client)->interactions, entry, pointers);
    }
    free((void*)bodystr);
}

static void parse_command(alloclient *client, cJSON *cmdrep)
{
    const char *cmdname = nonnull(cJSON_GetArrayItem(cmdrep, 0))->valuestring;
    if(strcmp(cmdname, "interaction") == 0) {
        parse_interaction(client, cmdrep);
    } else {
        printf("Unknown command: %s\n", cmdrep->string);
    }
}

static void parse_packet_from_channel(alloclient *client, ENetPacket *packet, allochannel channel)
{
    // Stupid: protocol says "end with newline". Either way we can't guarantee that remote side
    // null terminates for us. Payload is always JSON so far. Let's always manually replace the
    // newline with a null and be done with it.
    packet->data[packet->dataLength-1] = 0;
    cJSON *cmdrep = cJSON_Parse((const char*)(packet->data));
    
    switch(channel) {
    case CHANNEL_STATEDIFFS:
        parse_statediff(client, cmdrep);
        break;
    case CHANNEL_COMMANDS: {
        parse_command(client, cmdrep);
        break; }
    default: break;
    }
    
    cJSON_Delete(cmdrep);
}

void alloclient_poll(alloclient *client)
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

void alloclient_set_intent(alloclient *client, allo_client_intent intent)
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
    cJSON_Delete(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, 0);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_STATEDIFFS, packet);
    free((void*)json);
}

void alloclient_send_interaction(
    alloclient *client,
    const char *interaction_type,
    const char *sender_entity_id,
    const char *receiver_entity_id,
    const char *request_id,
    const char *body
)
{
    cJSON *cmdrep = cjson_create_list(
        nonnull(cJSON_CreateString("interaction")),
        nonnull(cJSON_CreateString(interaction_type)),
        nonnull(cJSON_CreateString(sender_entity_id)),
        nonnull(cJSON_CreateString(receiver_entity_id)),
        nonnull(cJSON_CreateString(request_id)),
        nonnull(cJSON_Parse(body)),
        NULL
    );
    const char *json = cJSON_Print(cmdrep);
    cJSON_Delete(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_COMMANDS, packet);
    free((void*)json);
}

void alloclient_disconnect(alloclient *client, int reason)
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

bool announce(alloclient *client, const char *identity, const char *avatar_desc)
{
    cJSON *bodyobj = cjson_create_list(
        cJSON_CreateString("announce"),
        cJSON_CreateString("version"),
        cJSON_CreateNumber(1),
        cJSON_CreateString("identity"),
        cJSON_Parse(identity),
        cJSON_CreateString("spawn_avatar"),
        cJSON_Parse(avatar_desc),
        NULL
    );
    if(cJSON_GetArraySize(bodyobj) != 7)
    {
        fprintf(stderr, "Invalid identity or avatar_desc (must be json), disconnecting.\n");
        return false;
    }
    const char *body = cJSON_Print(bodyobj);
    alloclient_send_interaction(
        client,
        "request",
        "",
        "place",
        "ANN0",
        body
    );
    cJSON_Delete(bodyobj);
    free((void*)body);
    return true;
}

alloclient *allo_connect(const char *url, const char *identity, const char *avatar_desc)
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
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->host = host;
    _internal(client)->peer = peer;
    LIST_INIT(&client->state.entities);

    if(!announce(client, identity, avatar_desc))
    {
        alloclient_disconnect(client, 1);
        return NULL;
    }
    

    return client;
}

allo_interaction *alloclient_pop_interaction(alloclient *client)
{
    allo_interaction *interaction = NULL;
    interaction_queue *first = _internal(client)->interactions.lh_first;
    if(first) {
        interaction = first->interaction;
        LIST_REMOVE(first, pointers);
        free(first);
    }
    return interaction;
}