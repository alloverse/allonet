#include "_client.h"
#include <allonet/arr.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../util.h"
#include "../clientproxy.h"
#include "../asset.h"
#include <allonet/assetstore.h>


#if !defined(NDEBUG) && (defined(__clang__) || defined(__GNUC__))
    //#define nonnull(x) (assert((x) != NULL), x)
    #define nonnull(x) x
#else
    #define nonnull(x) x
#endif

static void send_latest_intent(alloclient* client);
static void send_clock_request(alloclient *client);
void alloclient_ack_rev(alloclient *client, int64_t rev);

static void _handle_parsed_statediff(alloclient *client, cJSON *cmd, cJSON *staterep, allo_state_diff *diff);
int64_t alloclient_parse_statediff(alloclient *client, cJSON *cmd)
{
    if(client->raw_state_delta_callback) 
    {
        client->raw_state_delta_callback(client, cmd);
        return 0;
    }

    allo_state_diff diff;
    allo_state_diff_init(&diff);
    cJSON *staterep = allo_delta_apply(&_internal(client)->history, cmd, &diff, (allo_statediff_handler)_handle_parsed_statediff, client);    
    allo_state_diff_free(&diff);

    int64_t rev = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(staterep, "revision"));
    return rev;
}

static void _handle_parsed_statediff(alloclient *client, cJSON *cmd, cJSON *staterep, allo_state_diff *diff)
{
    if(staterep == NULL)
    {
        int64_t patch_from = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(cmd, "patch_from"));
        fprintf(stderr, "alloclient[W](%s): Received unmergeable state from %lld (I'm %lld), requesting full state\n",
            _internal(client)->avatar_id,
            (long long int)patch_from,
            (long long int)_internal(client)->history.latest_revision
        );
        _internal(client)->latest_intent->ack_state_rev = 0;
        return;
    }

    int64_t rev = nonnull(cJSON_GetObjectItemCaseSensitive(staterep, "revision"))->valueint;
    alloclient_ack_rev(client, rev);

    const cJSON *entities = nonnull(cJSON_GetObjectItemCaseSensitive(staterep, "entities"));

    for(size_t i = 0; i < diff->new_entities.length; i++)
    {
        allo_entity *entity = entity_create(diff->new_entities.data[i]);
        LIST_INSERT_HEAD(&client->_state.entities, entity, pointers);
    }
    for(size_t i = 0; i < diff->deleted_entities.length; i++) {
        const char *eid = diff->deleted_entities.data[i];
        allo_entity *to_delete = state_get_entity(&client->_state, eid);
        LIST_REMOVE(to_delete, pointers);
        entity_destroy(to_delete);
    }
    
    // update or create entities
    cJSON *edesc = NULL;
    cJSON_ArrayForEach(edesc, entities) {
        const char *entity_id = nonnull(cJSON_GetObjectItemCaseSensitive(edesc, "id"))->valuestring;
        
        cJSON *components = nonnull(cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(edesc, "components"), 1));
        allo_entity *entity = state_get_entity(&client->_state, entity_id);
        cJSON_Delete(entity->components);
        entity->components = components;
    }

    _alloclient_media_handle_statediff(client, diff);

    // to debug state changes:
    //allo_state_diff_dump(diff);

    if(client->state_callback)
    {
        client->state_callback(client, &client->_state, diff);
    }
}

void alloclient_ack_rev(alloclient *client, int64_t rev)
{
    _internal(client)->latest_intent->ack_state_rev = client->_state.revision = rev;
}


static bool parse_interaction(alloclient *client, cJSON *inter_json)
{
    const char *type = nonnull(cJSON_GetArrayItem(inter_json, 1))->valuestring;
    const char *from = nonnull(cJSON_GetArrayItem(inter_json, 2))->valuestring;
    const char *to = nonnull(cJSON_GetArrayItem(inter_json, 3))->valuestring;
    const char *request_id = nonnull(cJSON_GetArrayItem(inter_json, 4))->valuestring;
    cJSON *body = nonnull(cJSON_GetArrayItem(inter_json, 5));
    const char *bodystr = cJSON_Print(body);
    if(strcmp(cJSON_GetStringValue(cJSON_GetArrayItem(body, 0)), "announce") == 0)
    {
        if(strcmp(cJSON_GetStringValue(cJSON_GetArrayItem(body, 1)), "error") == 0)
        {
            int reason = cJSON_GetNumberValue(cJSON_GetArrayItem(body, 2));
            const char *message = cJSON_GetStringValue(cJSON_GetArrayItem(body, 3));
            fprintf(stderr, "Place rejected our announce: %s (%d)\n", message, reason);
            // ask disconnection callback to deallocate us
            client->disconnected_callback(client, reason, message);
            return false;
        }
        else
        {
            _internal(client)->avatar_id = allo_strdup(cJSON_GetStringValue(cJSON_GetArrayItem(body, 1)));
        }
    }
    allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
    if(!client->interaction_callback || client->interaction_callback(client, interaction))
    {
        allo_interaction_free(interaction);
    }
    free((void*)bodystr);
    return true;
}

static bool parse_command(alloclient *client, cJSON *cmdrep)
{
    const char *cmdname = nonnull(cJSON_GetArrayItem(cmdrep, 0))->valuestring;
    if(strcmp(cmdname, "interaction") == 0) {
        return parse_interaction(client, cmdrep);
    } else {
        printf("Unknown command: %s\n", cmdrep->string);
        return true;
    }
}

static void parse_clock(alloclient *client, cJSON *response)
{
    double now = get_ts_monod();
    double then = cJSON_GetObjectItemCaseSensitive(response, "client_time")->valuedouble;
    double server_time = cJSON_GetObjectItemCaseSensitive(response, "server_time")->valuedouble;
    double roundtrip = now - then;
    double latency = client->clock_latency = roundtrip / 2.0;
    double delta = client->clock_deltaToServer = server_time + roundtrip - now;
    if(client->clock_callback)
    {
        client->clock_callback(client, latency, delta);
    }
}

static void _asset_send_func(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user) {
    alloclient *client = (alloclient*)user;
    ENetPeer *peer = _internal(client)->peer;
    
    if (peer == NULL) {
        printf("Asset: Not connected yet\n");
        return;
    }
    
    ENetPacket *packet = asset_build_enet_packet(mid, header, data, data_length);
    enet_peer_send(peer, CHANNEL_ASSETS, packet);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_ASSETS], packet->dataLength);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
}

static void _asset_request_bytes_func(const char *asset_id, size_t offset, size_t length, void *user) {
    alloclient *client = (alloclient *)user;
    if (client->asset_request_bytes_callback) {
        client->asset_request_bytes_callback(client, asset_id, offset, length);
    } else {
        uint8_t *buffer = malloc(length);
        assert(buffer);
        size_t total_size = 0;
        int read_bytes = assetstore_read(&(_internal(client)->assets), asset_id, offset, buffer, length, &total_size);
        
        if (read_bytes > 0) {
            asset_deliver_bytes(asset_id, buffer, offset, read_bytes, total_size, _asset_send_func, user);
        } else {
            asset_deliver_bytes(asset_id, NULL, offset, 0, 0, _asset_send_func, user);
        }
        
        free(buffer);
    }
}

static int _asset_write_func(const char *asset_id, const uint8_t *buffer, size_t offset, size_t length, size_t total_size, void *user) {
    alloclient *client = (alloclient *)user;
    if (client->asset_receive_callback) {
        client->asset_receive_callback(client, asset_id, buffer, offset, length, total_size);
        return length;
    } else {
        return assetstore_write(&(_internal(client)->assets), asset_id, offset, buffer, length, total_size);
    }
}

static void _asset_state_callback_func(const char *asset_id, asset_state state, void *user) {
    alloclient *client = (alloclient *)user;
    if (client->asset_state_callback) {
        client->asset_state_callback(client, asset_id, state);
    }
}

static void handle_assets(const uint8_t *data, size_t data_length, alloclient *client) {
    asset_handle(
        data, data_length,
        _asset_request_bytes_func,
        _asset_write_func,
        _asset_send_func,
        _asset_state_callback_func,
        (void*)client
    );
}

static bool parse_packet_from_channel(alloclient *client, ENetPacket *packet, allochannel channel)
{
    bool remain = true;
    bitrate_increment_received(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
    if (channel < CHANNEL_COUNT) {
        bitrate_increment_received(&allo_statistics.channel_rates[channel], packet->dataLength);
    }
    
    switch(channel) {
    case CHANNEL_STATEDIFFS: {
        cJSON *cmdrep = cJSON_ParseWithLengthOpts((const char*)(packet->data), packet->dataLength, NULL, 0);
        if(!cmdrep) {
            fprintf(stderr, "alloclient: unparseable statediff:\n");
            fprintf(stderr, "%s\n", packet->data);
            assert(cmdrep);
            return true;
        }
        //printf("alloclient(%s): My latest rev is %zd. Receiving delta %s\n", _internal(client)->avatar_id, _internal(client)->latest_intent->ack_state_rev, packet->data);
        alloclient_parse_statediff(client, cmdrep);
        break; }
    case CHANNEL_COMMANDS: {
        cJSON *cmdrep = cJSON_ParseWithLengthOpts((const char*)(packet->data), packet->dataLength, NULL, 0);
        remain = parse_command(client, cmdrep);
        cJSON_Delete(cmdrep);
        break; }
    case CHANNEL_ASSETS: {
        handle_assets(packet->data, packet->dataLength, client);
        } break;
    case CHANNEL_AUDIO:
    case CHANNEL_VIDEO: {
        _alloclient_parse_media(client, (unsigned char*)packet->data, packet->dataLength);
        break; }
    case CHANNEL_CLOCK: {
        cJSON *response = cJSON_ParseWithLengthOpts((const char*)(packet->data), packet->dataLength, NULL, 0);
        parse_clock(client, response);
        cJSON_Delete(response);
        break; }
    case CHANNEL_COUNT: assert(false);
    }
    return remain;
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
    int64_t servicing_timeout = deadline - ts < 0 ? 0 : deadline - ts;
    ENetEvent event;
    bool any_messages = false;
    ENetPacket *lastDiffPacket = 0;
    while (enet_host_service(_internal(client)->host, & event, servicing_timeout) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
                // hack: server pushes diffs in a steady pace. If we get some
                // congestion on network or cpu we might receive a bunch of
                // diffs with overlapping revisions. So let's just skip all but
                // the last one.
                if ((allochannel)event.channelID == CHANNEL_STATEDIFFS) {
                    if (lastDiffPacket) enet_packet_destroy(lastDiffPacket);
                    lastDiffPacket = event.packet;
                } else {
                    bool remain = parse_packet_from_channel(client, event.packet, (allochannel)event.channelID);
                    enet_packet_destroy(event.packet);
                    if(!remain)
                    {
                        // again disconnected, again deallocated, stop working.
                        // We might now be deallocated (that's the standard thing to do in the disconnected callback).
                        // Stop processing events.
                        return true;
                    }
                }
            break;
        
        case ENET_EVENT_TYPE_CONNECT:
                break;
        case ENET_EVENT_TYPE_DISCONNECT:
            fprintf(stderr, "alloclient: disconnected by remote peer or timeout\n");
            if (client->disconnected_callback) {
                client->disconnected_callback(client, alloerror_connection_lost, "Lost connection to Place");
            }
            // We might now be deallocated (that's the standard thing to do in the disconnected callback).
            // Stop processing events.
            return true;
        default: break;
        }
        any_messages = true;
        ts = get_ts_mono();
        servicing_timeout = deadline - ts < 0 ? 0 : deadline - ts;
    }
    
    if (lastDiffPacket) {
        parse_packet_from_channel(client, lastDiffPacket, CHANNEL_STATEDIFFS);
        enet_packet_destroy(lastDiffPacket);
        lastDiffPacket = 0;
    }
    return any_messages;
}

void alloclient_set_intent(alloclient* client, const allo_client_intent *intent)
{
    client->alloclient_set_intent(client, intent);
}
static void _alloclient_set_intent(alloclient* client, const allo_client_intent *intent)
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
    ENetPacket *packet = enet_packet_create(NULL, jsonlength, 0 /* unreliable */);
    memcpy(packet->data, json, jsonlength);
    enet_peer_send(_internal(client)->peer, CHANNEL_STATEDIFFS, packet);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_STATEDIFFS], packet->dataLength);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
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
    ENetPacket *packet = enet_packet_create(NULL, jsonlength, 0 /* unreliable */);
    memcpy(packet->data, json, jsonlength);
    enet_peer_send(_internal(client)->peer, CHANNEL_CLOCK, packet);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_CLOCK], packet->dataLength);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
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
    ENetPacket *packet = enet_packet_create(NULL, jsonlength, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, json, jsonlength);
    enet_peer_send(_internal(client)->peer, CHANNEL_COMMANDS, packet);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COMMANDS], packet->dataLength);
    bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
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
        allo_delta_clear(&_internal(client)->history);
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
        cJSON_CreateNumber(GetAllonetProtocolVersion()),
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
            (size_t)(port-hostandport-1) : // dup up until and excluding the colon
            slash ? // otherwise, if there's a slash...
                (size_t)(slash-hostandport) : // dup up until and excluding the slash
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

void alloclient_send_video(alloclient *client, int32_t track_id, allopicture *picture)
{
    client->alloclient_send_video(client, track_id, picture);
}



void alloclient_simulate(alloclient *client)
{
    client->alloclient_simulate(client);
}
static void _alloclient_simulate(alloclient *client)
{
  const allo_client_intent *intents[] = {_internal(client)->latest_intent};
  
  double now = alloclient_get_time(client);
  allo_state_diff diff; allo_state_diff_init(&diff);
  allo_simulate(&client->_state, intents, 1, now, &diff);
  if (client->state_callback)
  {
    client->state_callback(client, &client->_state, &diff);
  }
  allo_state_diff_free(&diff);
}

double alloclient_get_time(alloclient* client)
{
    return client->alloclient_get_time(client);
}
static double _alloclient_get_time(alloclient* client)
{
    return get_ts_monod() + client->clock_deltaToServer;
}


void alloclient_get_stats(alloclient* client, char *buffer, size_t bufferlen)
{
    client->alloclient_get_stats(client, buffer, bufferlen);
}
static void _alloclient_get_stats(alloclient* client, char *buffer, size_t bufferlen)
{
    if (client == NULL || _internal(client) == NULL || _internal(client)->peer == NULL)
    {
        snprintf(buffer, bufferlen, "(disconnected, no stats available)");
        return;
    }
    snprintf(buffer, bufferlen,
        "State: set:%u delta:%u\n"
        "Packets lost\t%d\n"
        "RTT\t%dms\t\n"
        "Packet throttle\t%.0f%%\n"
        ,
        allo_statistics.ndelta_set, allo_statistics.ndelta_merge,
        _internal(client)->peer->packetsLost,
        _internal(client)->peer->roundTripTime,
        (_internal(client)->peer->packetThrottle / (double)ENET_PEER_PACKET_THROTTLE_SCALE)*100.0
    );
    
    double time = alloclient_get_time(client);
    static char*names[CHANNEL_COUNT+1] = {"audio", "cmd", "diff", "asset", "video", "clock", "total"};
    for (int i = 0; i < CHANNEL_COUNT+1; i++) {
        int len = strlen(buffer);
        bufferlen -= len;
        buffer += len;
        struct bitrate_deltas_t deltas = bitrate_deltas(&allo_statistics.channel_rates[i], time);
        snprintf(buffer, bufferlen,
                 "ch%d-%s\t(%zu, %zu) %.3f/%.3fkbps\n"
                 ,
                 i, names[i], deltas.sent_packet_count, deltas.received_packet_count, deltas.sent_bytes/1024.0, deltas.received_bytes/1024.0
                 );
    }
    
    int len = strlen(buffer);
    bufferlen -= len;
    buffer += len;
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    allo_media_get_stats(&shared->media_tracks, buffer, bufferlen);
    _alloclient_internal_shared_end(client);
}

void alloclient_asset_request(alloclient* client, const char* asset_id, const char* entity_id) {
    client->alloclient_asset_request(client, asset_id, entity_id);
}
static void _alloclient_asset_request(alloclient* client, const char* asset_id, const char* entity_id) {
    asset_request(asset_id, entity_id, _asset_send_func, (void*)client);
}

void alloclient_asset_send(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size) {
    client->alloclient_asset_send(client, asset_id, data, offset, length, total_size);
}
static void _alloclient_asset_send(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size) {
    
    asset_deliver_bytes(asset_id, data, offset, length, total_size, _asset_send_func, (void*)client);
}

alloclient *_alloclient_create()
{
    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->_internal = calloc(1, sizeof(alloclient_internal));
    _internal(client)->latest_intent = allo_client_intent_create();
    assetstore_init(&(_internal(client)->assets));
    
    scheduler_init(&_internal(client)->jobs);
    
    LIST_INIT(&client->_state.entities);
    
    client->alloclient_connect = _alloclient_connect;
    client->alloclient_disconnect = _alloclient_disconnect;
    client->alloclient_poll = _alloclient_poll;
    client->alloclient_send_interaction = _alloclient_send_interaction;
    client->alloclient_set_intent = _alloclient_set_intent;
    client->alloclient_send_audio = _alloclient_send_audio;
    client->alloclient_send_video = _alloclient_send_video;
    client->alloclient_simulate = _alloclient_simulate;
    client->alloclient_get_time = _alloclient_get_time;
    client->alloclient_get_stats = _alloclient_get_stats;
    client->asset_request_bytes_callback = NULL;
    client->asset_receive_callback = NULL;
    client->asset_state_callback = NULL;
    
    
    // assets
    client->alloclient_asset_request = _alloclient_asset_request;
    client->alloclient_asset_send = _alloclient_asset_send;
    
    return client;
}

alloclient *alloclient_create(bool threaded) {
    if (threaded) {
        alloclient *network = _alloclient_create(); // will not have a shared
        alloclient *proxy = clientproxy_create(network); // will have a shared
        _internal(network)->shared = _internal(proxy)->shared; // share shared
        
        return proxy;
    }
    
    alloclient *client = _alloclient_create();
    alloclient_internal_shared *shared = malloc(sizeof(alloclient_internal_shared));
    mtx_init(&shared->lock, mtx_plain);
    arr_init(&shared->media_tracks);
    _internal(client)->shared = shared;
    
    return client;
}


void allopicture_free(allopicture *picture)
{
    if(picture->free)
    {
        picture->free(picture);
    }
    else
    {
        for(int i = 0; i < ALLOPICTURE_MAX_PLANE_COUNT; i++)
        {
            free(picture->planes[i].monochrome);
        }
        free(picture);
    }
}

int allopicture_bpp(allopicture_format fmt)
{
    switch(fmt)
    {
        case allopicture_format_rgb1555:
        case allopicture_format_rgb565:
            return 2;
        case allopicture_format_rgba8888:
        case allopicture_format_xrgb8888:
        case allopicture_format_bgra8888:
            return 4;
    }
    return 0;
}
