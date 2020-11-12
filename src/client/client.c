#include "_client.h"
#include <allonet/arr.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../util.h"
#include "../clientproxy.h"


#if !defined(NDEBUG) && (defined(__clang__) || defined(__GNUC__))
    #define nonnull(x) ({ typeof(x) xx = (x); assert(xx != NULL); xx; })
#else
    #define nonnull(x) x
#endif

static void send_latest_intent(alloclient* client);
static void send_clock_request(alloclient *client);

void alloclient_parse_statediff(alloclient *client, cJSON *cmd)
{
    if(client->raw_state_delta_callback) 
    {
        client->raw_state_delta_callback(client, cmd);
        return;
    }

    int64_t patch_from = cjson_get_int64_value(cJSON_GetObjectItem(cmd, "patch_from"));
    cJSON *staterep = allo_delta_apply(&_internal(client)->history, cmd);
    if(staterep == NULL)
    {
        fprintf(stderr, "alloclient[W](%s): Received unmergeable state from %zd (I'm %zd), requesting full state\n",
            _internal(client)->avatar_id,
            patch_from,
            _internal(client)->history.latest_revision
        );
        _internal(client)->latest_intent->ack_state_rev = 0;
        return;
    }

    int64_t rev = nonnull(cJSON_GetObjectItem(staterep, "revision"))->valueint;
    _internal(client)->latest_intent->ack_state_rev = client->_state.revision = rev;

    const cJSON *entities = nonnull(cJSON_GetObjectItem(staterep, "entities"));
    
    // keep track of entities that aren't mentioned in the incoming list
    cJSON *deletes = cJSON_CreateArray();
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &client->_state.entities, pointers)
    {
        cJSON_AddItemToArray(deletes, cJSON_CreateString(entity->id));
    }
    
    // update or create entities
    cJSON *edesc = NULL;
    cJSON_ArrayForEach(edesc, entities) {
        const char *entity_id = nonnull(cJSON_GetObjectItem(edesc, "id"))->valuestring;
        cjson_delete_from_array(deletes, entity_id);
        
        cJSON *components = nonnull(cJSON_Duplicate(cJSON_GetObjectItem(edesc, "components"), 1));
        allo_entity *entity = state_get_entity(&client->_state, entity_id);
        if(!entity) {
            entity = entity_create(entity_id);
            LIST_INSERT_HEAD(&client->_state.entities, entity, pointers);
        }
        
        cJSON_Delete(entity->components);
        entity->components = components;
    }

    // now, delete old entities
    entity = client->_state.entities.lh_first;
    while(entity)
    {
        allo_entity *to_delete = entity;
        entity = entity->pointers.le_next;
        if(cjson_find_in_array(deletes, to_delete->id) != -1) {
            LIST_REMOVE(to_delete, pointers);
            
            // if we find a decoder linked to the entity we remove it
            cJSON *media = cJSON_GetObjectItem(to_delete->components, "live_media");
            cJSON *track_id = cJSON_GetObjectItem(media, "track_id");
            if (media && track_id) {
                fprintf(stderr, "Destroying a linked decoder\n");
                _alloclient_decoder_destroy_for_track(client, track_id->valueint);
            }
            
            entity_destroy(to_delete);
        }
    }
    cJSON_Delete(deletes);

    if(client->state_callback)
    {
        client->state_callback(client, &client->_state);
    }
}


static void parse_interaction(alloclient *client, cJSON *inter_json)
{
    const char *type = nonnull(cJSON_GetArrayItem(inter_json, 1))->valuestring;
    const char *from = nonnull(cJSON_GetArrayItem(inter_json, 2))->valuestring;
    const char *to = nonnull(cJSON_GetArrayItem(inter_json, 3))->valuestring;
    const char *request_id = nonnull(cJSON_GetArrayItem(inter_json, 4))->valuestring;
    cJSON *body = nonnull(cJSON_GetArrayItem(inter_json, 5));
    const char *bodystr = cJSON_Print(body);
    if(strcmp(cJSON_GetStringValue(cJSON_GetArrayItem(body, 0)), "announce") == 0)
    {
        _internal(client)->avatar_id = allo_strdup(cJSON_GetStringValue(cJSON_GetArrayItem(body, 1)));
    }
    allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
    if(!client->interaction_callback || client->interaction_callback(client, interaction))
    {
        allo_interaction_free(interaction);
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

static void parse_clock(alloclient *client, cJSON *response)
{
    double now = get_ts_monod();
    double then = cJSON_GetObjectItem(response, "client_time")->valuedouble;
    double server_time = cJSON_GetObjectItem(response, "server_time")->valuedouble;
    double roundtrip = now - then;
    double latency = client->clock_latency = roundtrip / 2.0;
    double delta = client->clock_deltaToServer = server_time + roundtrip - now;
    if(client->clock_callback)
    {
        client->clock_callback(client, latency, delta);
    }
}

static void parse_packet_from_channel(alloclient *client, ENetPacket *packet, allochannel channel)
{
    // Stupid: protocol says "end with newline". Either way we can't guarantee that remote side
    // null terminates for us. Payload is always JSON so far. Let's always manually replace the
    // newline with a null and be done with it.
    packet->data[packet->dataLength-1] = 0;
    
    switch(channel) {
    case CHANNEL_STATEDIFFS: { 
        cJSON *cmdrep = cJSON_Parse((const char*)(packet->data));
        if(!cmdrep) {
            fprintf(stderr, "alloclient: unparseable statediff:\n");
            fprintf(stderr, "%s\n", packet->data);
            assert(cmdrep);
            return;
        }
        //printf("alloclient(%s): My latest rev is %zd. Receiving delta %s\n", _internal(client)->avatar_id, _internal(client)->latest_intent->ack_state_rev, packet->data);
        alloclient_parse_statediff(client, cmdrep);
        break; }
    case CHANNEL_COMMANDS: {
        cJSON *cmdrep = cJSON_Parse((const char*)(packet->data));
        parse_command(client, cmdrep);
        cJSON_Delete(cmdrep);
        break; }
    case CHANNEL_ASSETS: {
        //parse_asset(client, (char*)packet->data, packet->dataLength);
        } break;
    case CHANNEL_MEDIA: {
        _alloclient_parse_media(client, (unsigned char*)packet->data, packet->dataLength-1);
        break; }
    case CHANNEL_CLOCK: {
        cJSON *response = cJSON_Parse((const char*)(packet->data));
        parse_clock(client, response);
        cJSON_Delete(response);
        break; }
    }
}

bool alloclient_poll(alloclient *client, int timeout_ms)
{
    return client->alloclient_poll(client, timeout_ms);
}
bool _alloclient_poll(alloclient *client, int timeout_ms)
{
    if(_internal(client)->peer == NULL)
    {
        return false;
    } 

    int64_t ts = get_ts_mono();
    int64_t deadline = ts + timeout_ms;

    // send clock sync request at maximum 1hz
    if(ts > _internal(client)->latest_clockreq_ts + 1000)
    {
        send_clock_request(client);
        _internal(client)->latest_clockreq_ts = ts;
    }
    
    // send intent at maximum 20hz
    if(ts > _internal(client)->latest_intent_ts + (1000/20))
    {
        send_latest_intent(client);
        _internal(client)->latest_intent_ts = ts;
    }
    
    ts = get_ts_mono();
    int64_t servicing_timeout = MAX(deadline - ts, 0);
    ENetEvent event;
    bool any_messages = false;
    while (enet_host_service(_internal(client)->host, & event, servicing_timeout) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            parse_packet_from_channel(client, event.packet, (allochannel)event.channelID);
            enet_packet_destroy (event.packet);
            break;
        
        case ENET_EVENT_TYPE_DISCONNECT:
            fprintf(stderr, "alloclient: disconnected by remote peer or timeout\n");
            if (client->disconnected_callback) {
                client->disconnected_callback(client, alloerror_connection_lost, "Lost connection to Place");
                // We might now be deallocated (that's the standard thing to do in the disconnected callback).
                // Stop processing events.
                return true;
            }
        default: break;
        }
        any_messages = true;
        ts = get_ts_mono();
        servicing_timeout = MAX(deadline - ts, 0);
    }
    return any_messages;
}

void alloclient_set_intent(alloclient* client, allo_client_intent *intent)
{
    client->alloclient_set_intent(client, intent);
}
static void _alloclient_set_intent(alloclient* client, allo_client_intent *intent)
{
    // this one is set internally, so don't overwrite it
    int64_t ack_state_rev = _internal(client)->latest_intent->ack_state_rev;
    allo_client_intent_clone(intent, _internal(client)->latest_intent);
    _internal(client)->latest_intent->ack_state_rev = ack_state_rev;
}

static void send_latest_intent(alloclient *client)
{
    allo_client_intent *intent = _internal(client)->latest_intent;

    //printf("%s Sending latest intent, ACKing rev %zd.\n", _internal(client)->avatar_id, _internal(client)->latest_intent->ack_state_rev);

    cJSON *cmdrep = cjson_create_object(
        "cmd", cJSON_CreateString("intent"),
        "intent", allo_client_intent_to_cjson(intent),
        NULL
    );
    const char *json = cJSON_Print(cmdrep);
    cJSON_Delete(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, 0 /* unreliable */);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_STATEDIFFS, packet);
    free((void*)json);
}

static void send_clock_request(alloclient *client)
{
    cJSON *cmdrep = cjson_create_object(
        "client_time", cJSON_CreateNumber(get_ts_monod()),
        NULL
    );
    const char *json = cJSON_Print(cmdrep);
    cJSON_Delete(cmdrep);

    int jsonlength = strlen(json);
    ENetPacket *packet = enet_packet_create(NULL, jsonlength+1, 0 /* unreliable */);
    memcpy(packet->data, json, jsonlength);
    ((char*)packet->data)[jsonlength] = '\n';
    enet_peer_send(_internal(client)->peer, CHANNEL_CLOCK, packet);
    free((void*)json);
}

void alloclient_send_interaction(alloclient *client, allo_interaction *interaction)
{
    client->alloclient_send_interaction(client, interaction);
}
static void _alloclient_send_interaction(alloclient *client, allo_interaction *interaction)
{
    cJSON *cmdrep = cjson_create_list(
        nonnull(cJSON_CreateString("interaction")),
        nonnull(cJSON_CreateString(interaction->type)),
        nonnull(cJSON_CreateString(interaction->sender_entity_id)),
        nonnull(cJSON_CreateString(interaction->receiver_entity_id)),
        nonnull(cJSON_CreateString(interaction->request_id)),
        nonnull(cJSON_Parse(interaction->body)),
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
    client->alloclient_disconnect(client, reason);
}
static void _alloclient_disconnect(alloclient *client, int reason)
{
    if (_internal(client)) {
        if (_internal(client)->peer && _internal(client)->peer->state != ENET_PEER_STATE_DISCONNECTED)
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
                    fprintf(stderr, "alloclient: successfully disconnected\n");
                    break;
                }
                now = get_ts_mono();
            }
            if(event.type != ENET_EVENT_TYPE_DISCONNECT) {
                fprintf(stderr, "alloclient: disconnection timed out; just deallocating\n");
            }
        } else {
            fprintf(stderr, "alloclient: Already disconnected; just deallocating\n");
        }
        enet_host_destroy(_internal(client)->host);
        opus_encoder_destroy(_internal(client)->opus_encoder);
        allo_client_intent_free(_internal(client)->latest_intent);
        allo_delta_destroy(&_internal(client)->history);
        free(_internal(client)->avatar_id);
        free(_internal(client));
    }
    
    free(client);
}

static bool announce(alloclient *client, const char *identity, const char *avatar_desc)
{
    cJSON *bodyobj = cjson_create_list(
        cJSON_CreateString("announce"),
        cJSON_CreateString("version"),
        cJSON_CreateNumber(ALLO_PROTOCOL_VERSION),
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
    allo_interaction *announce = allo_interaction_create(
        "request",
        "",
        "place",
        "ANN0",
        body
    );
    alloclient_send_interaction(client, announce);
    allo_interaction_free(announce);
    cJSON_Delete(bodyobj);
    free((void*)body);
    return true;
}

bool alloclient_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc)
{
    return client->alloclient_connect(client, url, identity, avatar_desc);
}
static bool _alloclient_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc)
{
    ENetHost * host;
    host = enet_host_create (NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        CHANNEL_COUNT /* allow up CHANNEL_COUNT channels to be used */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of incoming bandwidth */
    );
    if (host == NULL)
    {
        fprintf (stderr, 
                "An error occurred while trying to create an ENet client host.\n");
        return false;
    }

    // parse the url in the form of alloplace://hostname{:port}{/}
    if (strstr(url, "alloplace://") == 0) {
        fprintf(stderr, "Not an alloplace URL\n");
        return false;
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

    printf("Connecting to %s:%d (as %s)\n", justhost, address.port, identity);
    

    ENetPeer *peer;
    peer = enet_host_connect (host, &address, CHANNEL_COUNT, 0);    
    if (peer == NULL)
    {
        fprintf (stderr, 
                    "No available peers for initiating an ENet connection.\n");
        free(justhost);
        free(justport);
        return false;
    }

    ENetEvent event;
    if (enet_host_service(host, & event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        printf("Connection to %s:%d (as %s) succeeded.\n", justhost, address.port, identity);
    }
    else
    {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset (peer);
        printf("Connection to %s:%d (as %s) failed.\n", justhost, address.port, identity);
        free(justhost);
        free(justport);
        return false;
    }
    free(justhost);
    free(justport);

    enet_peer_timeout(peer, 3, 2000, 6000);

    _internal(client)->host = host;
    _internal(client)->peer = peer;

    int error;
    _internal(client)->opus_encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &error);
    if(_internal(client)->opus_encoder == NULL)
    {
        alloclient_disconnect(client, alloerror_initialization_failure);
        fprintf(stderr, "Encoder creation failed: %d.", error);
        return false;
    }

    if(!announce(client, identity, avatar_desc))
    {
        alloclient_disconnect(client, alloerror_initialization_failure);
        return false;
    }
    

    return true;
}

void alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount)
{
    client->alloclient_send_audio(client, track_id, pcm, frameCount);
}


void alloclient_simulate(alloclient *client)
{
    client->alloclient_simulate(client);
}
static void _alloclient_simulate(alloclient *client)
{
  const allo_client_intent *intents[] = {_internal(client)->latest_intent};
  
  allo_simulate(&client->_state, intents, 1, alloclient_get_time(client));
  if (client->state_callback)
  {
    client->state_callback(client, &client->_state);
  }
}

double alloclient_get_time(alloclient* client)
{
    return client->alloclient_get_time(client);
}
static double _alloclient_get_time(alloclient* client)
{
    return get_ts_monod() + client->clock_deltaToServer;
}



alloclient *alloclient_create(bool threaded)
{
    if(threaded)
    {
        return clientproxy_create();
    }

    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->latest_intent = allo_client_intent_create();
    
    scheduler_init(&_internal(client)->jobs);
    
    LIST_INIT(&client->_state.entities);

    client->alloclient_connect = _alloclient_connect;
    client->alloclient_disconnect = _alloclient_disconnect;
    client->alloclient_poll = _alloclient_poll;
    client->alloclient_send_interaction = _alloclient_send_interaction;
    client->alloclient_set_intent = _alloclient_set_intent;
    client->alloclient_send_audio = _alloclient_send_audio;
    client->alloclient_simulate = _alloclient_simulate;
    client->alloclient_get_time = _alloclient_get_time;
    
    return client;
}