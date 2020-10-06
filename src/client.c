#include <allonet/client.h>
#include <allonet/arr.h>
#include <enet/enet.h>
#include <opus.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "util.h"
#include <allonet/jobs.h>

#if !defined(NDEBUG) && (defined(__clang__) || defined(__GNUC__))
    #define nonnull(x) ({ typeof(x) xx = (x); assert(xx != NULL); xx; })
#else
    #define nonnull(x) x
#endif

#define DEBUG_AUDIO 0
typedef struct interaction_queue {
    allo_interaction *interaction;
    LIST_ENTRY(interaction_queue) pointers;
} interaction_queue;

typedef struct decoder_track {
    OpusDecoder *decoder;
    uint32_t track_id;
    FILE* debug;
    LIST_ENTRY(decoder_track) pointers;
} decoder_track;

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
    OpusEncoder *opus_encoder;
    allo_client_intent *latest_intent;
    int64_t latest_intent_ts;
    cJSON *jstate;
    LIST_HEAD(decoder_track_list, decoder_track) decoder_tracks;
    LIST_HEAD(interaction_queue_list, interaction_queue) interactions;
    scheduler jobs;
} alloclient_internal;

static alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

static void send_latest_intent(alloclient* client);

// TODO: nevyn: teach voxar cmake
// TODO: voxar: split out decoder.c

/// Find by track_id, else NULL
static decoder_track *decoder_find_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = NULL;
    LIST_FOREACH(dec, &_internal(client)->decoder_tracks, pointers)
    {
        if(dec->track_id == track_id)
        {
            return dec;
        }
    }
    return dec;
}

static decoder_track *decoder_find_or_create_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = decoder_find_for_track(client, track_id);
    if (dec) { return dec; }
    
    dec = (decoder_track*)calloc(1, sizeof(decoder_track));
    dec->track_id = track_id;
    int err;
    dec->decoder = opus_decoder_create(48000, 1, &err);
    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
        dec->debug = fopen(name, "wb");
        fprintf(stderr, "Opening decoder for %s\n", name);
    }
    assert(dec->decoder);
    LIST_INSERT_HEAD(&_internal(client)->decoder_tracks, dec, pointers);

    return dec;
}

static void decoder_destroy_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = decoder_find_for_track(client, track_id);
    if (dec == NULL) {
        fprintf(stderr, "A decoder for track_id %d was not found\n", track_id);
        fprintf(stderr, "Active decoder tracks:\n ");
        dec = NULL;
        LIST_FOREACH(dec, &_internal(client)->decoder_tracks, pointers)
        {
            fprintf(stderr, "%d, ", dec->track_id);
        }

        return;
    }
    assert(dec->decoder);

    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
        fprintf(stderr, "Closing decoder for %s\n", name);
        if (dec->debug) {
            fclose(dec->debug);
            dec->debug = NULL;
        }
    }
    dec->track_id = -1;
    opus_decoder_destroy(dec->decoder);
    dec->decoder = NULL;
    LIST_REMOVE(dec, pointers);

    free(dec);
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
        allo_entity *entity = state_get_entity(&client->state, entity_id);
        if(!entity) {
            entity = entity_create(entity_id);
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
            LIST_REMOVE(to_delete, pointers);
            
            // if we find a decoder linked to the entity we remove it
            cJSON *media = cJSON_GetObjectItem(to_delete->components, "live_media");
            cJSON *track_id = cJSON_GetObjectItem(media, "track_id");
            if (media && track_id) {
                fprintf(stderr, "Destroying a linked decoder\n");
                decoder_destroy_for_track(client, track_id->valueint);
            }
            
            entity_destroy(to_delete);
        }
    }
    cJSON_Delete(deletes);

    if(client->state_callback)
    {
        client->state_callback(client, &client->state);
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
    allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
    if(client->interaction_callback) {
        client->interaction_callback(client, interaction);
        allo_interaction_free(interaction);
    } else {
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


static void parse_media(alloclient *client, unsigned char *data, int length)
{
    uint32_t track_id;
    assert(length >= sizeof(track_id) + 3);
    memcpy(&track_id, data, sizeof(track_id));
    track_id = ntohl(track_id);
    data += sizeof(track_id);
    length -= sizeof(track_id);

    // todo: decode on another tread
    decoder_track *dec = decoder_find_or_create_for_track(client, track_id);
    const int maximumFrameCount = 5760; // 120ms as per documentation
    int16_t pcm[5760] = { 0 };
    int samples_decoded = opus_decode(dec->decoder, (unsigned char*)data, length, pcm, maximumFrameCount, 0);

    assert(samples_decoded >= 0);
    if (DEBUG_AUDIO && dec->debug) {
        fwrite(pcm, sizeof(int16_t), samples_decoded, dec->debug);
        fflush(dec->debug);
    }

    if(client->audio_callback) {
        client->audio_callback(client, track_id, pcm, samples_decoded);
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
        parse_statediff(client, cmdrep);
        cJSON_Delete(cmdrep);
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
        parse_media(client, (unsigned char*)packet->data, packet->dataLength-1);
    }
    }
}

bool alloclient_poll(alloclient *client, int timeout_ms)
{
    int64_t ts = get_ts_mono();
    int64_t deadline = ts + timeout_ms;
    
    // send intent at maximum 20hz
    if(ts > _internal(client)->latest_intent_ts + (1000/20)) {
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
    allo_client_intent_clone(intent, _internal(client)->latest_intent);
}

static void send_latest_intent(alloclient *client)
{
    allo_client_intent *intent = _internal(client)->latest_intent;

    cJSON *cmdrep = cjson_create_object(
        "cmd", cJSON_CreateString("intent"),
        "intent", allo_client_intent_to_cjson(intent),
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
    allo_interaction *interaction
)
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
                    fprintf(stderr, "alloclient: successfully disconnected");
                    break;
                }
                now = get_ts_mono();
            }
            if(event.type != ENET_EVENT_TYPE_DISCONNECT) {
                fprintf(stderr, "alloclient: disconnection timed out; just deallocating");
            }
        } else {
            fprintf(stderr, "alloclient: Already disconnected; just deallocating");
        }
        enet_host_destroy(_internal(client)->host);
        opus_encoder_destroy(_internal(client)->opus_encoder);
        allo_client_intent_free(_internal(client)->latest_intent);
        free(_internal(client));
    }
    
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

alloclient *alloclient_create(void)
{
    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->latest_intent = allo_client_intent_create();
    
    scheduler_init(&_internal(client)->jobs);
    
    LIST_INIT(&client->state.entities);
    
    return client;
}

bool allo_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc)
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

    printf("Connecting to %s:%d\n", justhost, address.port);
    free(justhost);
    free(justport);

    ENetPeer *peer;
    peer = enet_host_connect (host, &address, CHANNEL_COUNT, 0);    
    if (peer == NULL)
    {
        fprintf (stderr, 
                    "No available peers for initiating an ENet connection.\n");
        return false;
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
        return false;
    }

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

void alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount)
{
    assert(frameCount == 480 || frameCount == 960);
    
    const int headerlen = sizeof(int32_t); // track id header
    const int outlen = headerlen + frameCount*2 + 1; // theoretical max
    ENetPacket *packet = enet_packet_create(NULL, outlen, 0 /* unreliable */);
    assert(packet != NULL);
    int32_t big_track_id = htonl(track_id);
    memcpy(packet->data, &big_track_id, headerlen);

    int len = opus_encode (
        _internal(client)->opus_encoder, 
        pcm, frameCount,
        packet->data + headerlen, outlen - headerlen
    );

    if (len < 3) {  // error or DTX ("do not transmit")
        enet_packet_destroy(packet);
        if (len < 0) {
            fprintf(stderr, "Error encoding audio to send: %d", len);
        }
        return;
    }
    // +1 because stupid server code assumes all packets end with a newline... FIX THE DAMN PROTOCOL
    int ok = enet_packet_resize(packet, headerlen + len+1);
    assert(ok == 0);
    ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet);
    assert(ok == 0);
}

void alloclient_simulate(alloclient *client, double dt)
{
  const allo_client_intent *intents[] = {_internal(client)->latest_intent};
  allo_simulate(&client->state, dt, intents, 1);
  if (client->state_callback)
  {
    client->state_callback(client, &client->state);
  }
}

double alloclient_get_time(alloclient* client)
{
    // heh sync with server will come later...
    return get_ts_mono() / 1000.0;
}