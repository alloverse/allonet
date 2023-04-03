#include <allonet/server.h>
#include <enet/enet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <cJSON/cJSON.h>
#include "util.h"
#include <allonet/arr.h>
#include "asset.h"
#include "media/media.h"
#include <allonet/assetstore.h>



static inline char* formatClientIdAssetId(alloserver_client *client, const char *assetId) {
    if (client == NULL) return NULL;
    char *str;
    if (asprintf(&str, "client:%s %s", client->agent_id, assetId) > 0) return str;
    return NULL;
}

#if 1
#define server_log_asset(client, asset, ...) { \
    char *id = formatClientIdAssetId(client, asset); \
    allo_log(DEBUG, "server", id, __VA_ARGS__); \
    if(id)free(id); \
} while(false)
#else
#define server_log_asset(...)
#endif

static inline char* formatClientId(alloserver_client *client) {
    if (client == NULL) return NULL;
    char *str;
    if (asprintf(&str, "client:%s", client->agent_id) > 0) return str;
    return NULL;
}

#define server_log(type, client, ...) { \
    char *id = formatClientId(client); \
    allo_log(type, "server", id, __VA_ARGS__); \
    if(id)free(id); \
} while(false)

void allo_send(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *buf, int len);

typedef arr_t(ENetPeer *) PeerList;
// When a peer requests an asset that we do not have in cache
typedef struct wanted_asset {
    char *id; // asset id
    arr_t(ENetPeer *) recipients;  // The peers that wants the asset
    ENetPeer *source;// the peer we have choosen to download the asset from
    arr_t(ENetPeer *) remaining_potential_sources;
} wanted_asset;

typedef struct {
    ENetHost *enet;
    /// map from asset_id to list of client peers
    arr_t(wanted_asset*) wanted_assets;
    assetstore assetstore;
    allo_media_track_list media_tracks;
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
    allo_generate_id(client->agent_id, AGENT_ID_LENGTH+1);
    client->intent = allo_client_intent_create();
    _clientinternal(client)->peer = NULL;

    return client;
}

static void alloserv_client_free(alloserver_client *client)
{
    allo_client_intent_free(client->intent);
    cJSON_Delete(client->identity);
    free(_clientinternal(client));
    free(client);
}

static void handle_incoming_connection(alloserver *serv, ENetPeer* new_peer)
{
    alloserver_client *new_client = _client_create();
    char host[255] = {0};
    enet_address_get_host_ip(&new_peer->address, host, 254);
    server_log(INFO, new_client, "A new client connected from %s:%u as %s/%p.\n",
        host,
        new_peer->address.port,
        new_client->agent_id,
        (void*)new_client
    );
    
    _clientinternal(new_client)->peer = new_peer;
    _clientinternal(new_client)->peer->data = (void*)new_client;
    LIST_INSERT_HEAD(&serv->clients, new_client, pointers);

    // very hard timeout limits; change once clients actually send SYN
    enet_peer_timeout(new_peer, 0, 10000, 20000);
    if(serv->clients_callback) {
        serv->clients_callback(serv, new_client, NULL);
    }
}

wanted_asset *_asset_is_wanted(const char *asset_id, alloserver *server);
void _request_missing_asset(alloserver *server, alloserver_client *client, const char *asset_id);
void _forward_wanted_asset(const char *asset_id, alloserver *server, alloserver_client *client, wanted_asset *wanted);
void _forward_wanted_asset_failure(const char *asset_id, alloserver *server, alloserver_client *client, wanted_asset *wanted);
void _remove_client_from_wanted(alloserver *server, alloserver_client *client);

typedef struct asset_user {
    alloserver *server;
    alloserver_client *client;
    ENetPeer *peer;
} asset_user;


/// @param user must be an asset_user
static void _asset_send_func_broadcast(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user) {
    // Build packet and send to everyone except the client in user
    
    alloserver *server = ((asset_user *)user)->server;
    alloserver_client *client = ((asset_user *)user)->client;
    
    ENetPacket *packet = asset_build_enet_packet(mid, header, data, data_length);
    
    alloserver_client *other;
    LIST_FOREACH(other, &server->clients, pointers) {
        if (other == client) continue;
        ENetPeer *peer = _clientinternal(other)->peer;
        
        allo_enet_peer_send(peer, CHANNEL_ASSETS, packet);
    }
}
void _asset_send_func_peer(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user) {
    ENetPeer *peer = ((asset_user *)user)->peer;
    
    ENetPacket *packet = asset_build_enet_packet(mid, header, data, data_length);
    allo_enet_peer_send(peer, CHANNEL_ASSETS, packet);
}
void _asset_send_func(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user) {
    // Extract peer if one is not already set.
    // If user->peer is set it's probably for a destination other than user->client
    ENetPeer *peer = ((asset_user *)user)->peer;
    if (peer == NULL) {
        alloserver_client *client = ((asset_user *)user)->client;
        peer = _clientinternal(client)->peer;
    }
    asset_user usr = *(asset_user*)user;
    usr.peer = peer;
    
    _asset_send_func_peer(mid, header, data, data_length, &usr);
}

static void _asset_request_bytes_func(const char *asset_id, size_t offset, size_t length, void *user) {
    alloserver *server = ((asset_user *)user)->server;
    alloserver_client *client = ((asset_user *)user)->client;
    
    uint8_t *buffer = malloc(length);
    assert(buffer);
    
    size_t total_size = 0;
    int read_length = assetstore_read(&(_servinternal(server)->assetstore), asset_id, offset, buffer, length, &total_size);
    
    if (read_length > 0) {
        asset_deliver_bytes(asset_id, buffer, offset, read_length, total_size, _asset_send_func, user);
    } else {
        _request_missing_asset(server, client, asset_id);
    }
    free(buffer);
}

// we have acquired asset data from 'sender_peer' and want to store it.
static int _asset_write_func(const char *asset_id, const uint8_t *buffer, size_t offset, size_t length, size_t total_size, void *user) {
    alloserver *server = ((asset_user *)user)->server;
    alloserver_client *sender = ((asset_user *)user)->client;
    ENetPeer *sender_peer = _clientinternal(sender)->peer;
    wanted_asset *wanted = _asset_is_wanted(asset_id, server);
    
    if (wanted && (wanted->source == NULL || wanted->source == sender_peer)) {
        wanted->source = sender_peer;
        server_log_asset(sender, asset_id, "Received %d+%d=%d of %d bytes from %p", offset, length, offset+length, total_size, sender_peer);
        return assetstore_write(&(_servinternal(server)->assetstore), asset_id, offset, buffer, length, total_size);
    } else {
        return 0;
    }
}

// we now know the state of the asset (e g we KNOW nobody has it, or we have finished download it)
static void _asset_state_callback_func(const char *asset_id, asset_state state, void *user) {
    alloserver *server = ((asset_user *)user)->server;
    alloserver_client *client = ((asset_user *)user)->client;
    wanted_asset *wanted = _asset_is_wanted(asset_id, server);

    if (wanted == NULL) {
        server_log_asset(client, asset_id, "Unwanted asset got %d state callback", state);
        return;
    }

    if (state == asset_state_now_available) {
        server_log_asset(client, asset_id, "Forwarding asset", asset_id);
        // asset was completed
        // Ping registered peers by sending a first chunk.
        _forward_wanted_asset(asset_id, server, client, wanted);
    } else if (state == asset_state_now_unavailable) {
        ENetPeer *failed_source = _clientinternal(client)->peer;
        server_log_asset(client, asset_id, "Failed from source %p, %d remaining\n", failed_source, wanted->remaining_potential_sources.length-1);
        for (size_t source_index = 0; source_index < wanted->remaining_potential_sources.length; source_index++) {
            ENetPeer *source = wanted->remaining_potential_sources.data[source_index];
            if (failed_source == source) {
                arr_splice(&wanted->remaining_potential_sources, source_index, 1);
                break;
            }
        }
        // TODO: Add a timeout for if a client just never responds to the asset request?
        if(wanted->remaining_potential_sources.length == 0)
        {
            _forward_wanted_asset_failure(asset_id, server, client, wanted);
        }
    } else {
        server_log_asset(client, asset_id, "Unhandled asset state %d\n", state);
    }
}

static void handle_assets(const uint8_t *data, size_t data_length, alloserver *server, alloserver_client *client) {
    
    asset_user usr = { .server = server, .client = client };
    asset_handle(data, data_length, _asset_request_bytes_func, _asset_write_func, _asset_send_func, _asset_state_callback_func, (void*)&usr);
}

static void handle_incoming_data(alloserver *serv, alloserver_client *client, allochannel channel, ENetPacket *packet)
{
    bitrate_increment_received(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
    if (channel < CHANNEL_COUNT) {
        bitrate_increment_received(&allo_statistics.channel_rates[channel], packet->dataLength);
    }
    
    if (channel == CHANNEL_ASSETS) {
        handle_assets(packet->data, packet->dataLength, serv, client);
        return;
    }
    
    
    if(serv->raw_indata_callback && channel != CHANNEL_ASSETS)
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
    server_log(INFO, client, "%s/%p from %s:%d disconnected.\n", alloserv_describe_client(client), (void*)client, host, peer->address.port);

    // scan through the list of asset->peers and remove the peer where peeresent
    _remove_client_from_wanted(serv, client);
    
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
    (void)serv;
    ENetPacket *packet = enet_packet_create(
        NULL,
        len,
        (channel==CHANNEL_COMMANDS || channel==CHANNEL_ASSETS) ?
            ENET_PACKET_FLAG_RELIABLE :
            0
    );
    memcpy(packet->data, buf, len);
    allo_enet_peer_send(_clientinternal(client)->peer, channel, packet);
}

void alloserv_send_enet(alloserver *serv, alloserver_client *client, allochannel channel, ENetPacket *packet)
{
    (void)serv;
    allo_enet_peer_send(_clientinternal(client)->peer, channel, packet);
}

alloserver *allo_listen(int listenhost, int port)
{
    alloserver *serv = (alloserver*)calloc(1, sizeof(alloserver));
    serv->_internal = (alloserv_internal*)calloc(1, sizeof(alloserv_internal));
    arr_init(&_servinternal(serv)->wanted_assets);
    arr_init(&_servinternal(serv)->media_tracks);
    
    assetstore *assetstore = &(_servinternal(serv)->assetstore);
    asset_memstore_init(assetstore);
    asset_memstore_register_asset_nocopy(assetstore, "hello", (uint8_t*)"Hello World!", 13);
    
    srand((unsigned int)time(NULL));

    ENetAddress address;
    address.host = listenhost;
    address.port = port;
    char printable[255] = {0};
    enet_address_get_host_ip(&address, printable, 254);
    server_log(INFO, NULL, "Alloserv attempting listen on %s:%d...\n", printable, port);
    _servinternal(serv)->enet = enet_host_create(
        &address,
        allo_client_count_max,
        CHANNEL_COUNT,
        0,  // no ingress bandwidth limit
        0   // no egress bandwidth limit
    );
    if (_servinternal(serv)->enet == NULL)
    {
        server_log(ERROR, NULL, "An error occurred while trying to create an ENet server host.");
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
    (void)serv;
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

size_t alloserv_get_client_stats(alloserver* serv, alloserver_client *client, char *buffer, size_t bufferlen, bool header)
{
    ENetPeer *peer = _clientinternal(client)->peer;

    int entity_count = 0;
    allo_entity *ent;
    LIST_FOREACH(ent, &serv->state.entities, pointers) {
        if(strcmp(ent->owner_agent_id, client->agent_id) == 0)
            entity_count++;
    }

    int slen = 0;
    if(header)
    {
        slen += snprintf(buffer, bufferlen, "%s\n", alloserv_describe_client(client));
    }
    const char *indent = header?"\t":"";

    slen += snprintf(buffer+slen, bufferlen-slen,
        "%sEntities\t%d\n"
        "%sPackets lost\t%d\n"
        "%sRTT\t%dms\t\n"
        "%sPacket throttle\t%.0f%%\n"
        ,
        indent, entity_count,
        indent, peer->packetsLost,
        indent, peer->roundTripTime,
        indent, (peer->packetThrottle / (double)ENET_PEER_PACKET_THROTTLE_SCALE)*100.0
    );
    return slen;
}

void alloserv_get_stats(alloserver* server, char *buffer, size_t bufferlen)
{
    int offset = snprintf(buffer, bufferlen,
        "\tPeers\t%lu (limited %lu)\n"
        ,
        _servinternal(server)->enet->connectedPeers, _servinternal(server)->enet->bandwidthLimitedPeers
    );
    alloserver_client *client;

    LIST_FOREACH(client, &server->clients, pointers) {
        offset += alloserv_get_client_stats(server, client, buffer+offset, bufferlen-offset, true);
    }
    
    double time = get_ts_monod();
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
}

wanted_asset *_asset_is_wanted(const char *asset_id, alloserver *server) {
    alloserv_internal *sv = _servinternal(server);
    for (size_t i = 0; i < sv->wanted_assets.length; i++) {
        wanted_asset *wanted = sv->wanted_assets.data[i];
        if (strcmp(wanted->id, asset_id) == 0) {
            return wanted;
        }
    }
    return NULL;
}


void _request_missing_asset(alloserver *server, alloserver_client *client, const char *asset_id) {
    wanted_asset *wanted = _asset_is_wanted(asset_id, server);
    bool was_wanted = wanted != NULL;
    if(!wanted)
    {
        wanted = malloc(sizeof(wanted_asset));
        wanted->id = strdup(asset_id);
        wanted->source = NULL;
        arr_init(&wanted->recipients);
        arr_init(&wanted->remaining_potential_sources);
        arr_push(&_servinternal(server)->wanted_assets, wanted);
    }
    server_log_asset(client, asset_id, "%dth wanted to %p\n", wanted->recipients.length+1, _clientinternal(client)->peer);
    arr_push(&wanted->recipients, _clientinternal(client)->peer);
    
    // Broadcast request the asset unless it is already wanted
    if (!was_wanted) {
        alloserver_client *other;
        LIST_FOREACH(other, &server->clients, pointers) {
            if (other == client) continue;
            ENetPeer *peer = _clientinternal(other)->peer;
            arr_push(&wanted->remaining_potential_sources, peer);
        }
        server_log_asset(client, asset_id, "queued %d potential sources to get it\n", wanted->remaining_potential_sources.length);
        
        asset_user usr = { .server = server, .client = client };
        asset_request(asset_id, NULL, _asset_send_func_broadcast, &usr);
    }
}

static void free_wanted(wanted_asset *wanted)
{
    free(wanted->id);
    arr_free(&wanted->recipients);
    arr_free(&wanted->remaining_potential_sources);
    free(wanted);
}

void _forward_wanted_asset_failure(const char *asset_id, alloserver *server, alloserver_client *client, wanted_asset *wanted) {
    alloserv_internal *sv = _servinternal(server);
    server_log_asset(client, asset_id, "telling %d recipients that asset doesn't exist. potentials %d should be 0", wanted->recipients.length, wanted->remaining_potential_sources.length);

    for(size_t recipient_index = 0; recipient_index < wanted->recipients.length; recipient_index++)
    {
        ENetPeer *recipient = wanted->recipients.data[recipient_index];
        asset_user usr = { .server = server, .client = client, .peer = recipient };
        asset_deliver_error(asset_id, asset_not_available_error, "Server could not find asset", _asset_send_func, &usr);
    }
    for(size_t i = 0; i < sv->wanted_assets.length; i++)
    {
        if(sv->wanted_assets.data[i] == wanted)
        {   
            arr_splice(&sv->wanted_assets, i, 1);
            free_wanted(wanted);
            break;
        }
    }
}

void _forward_wanted_asset(const char *asset_id, alloserver *server, alloserver_client *client, wanted_asset *wanted) {
    alloserv_internal *sv = _servinternal(server);
    server_log_asset(client, asset_id, "telling %d recipients that asset DOES exist!\n", wanted->recipients.length);

    //TODO: Setup job to send to only send to a few peers at once.
    // deliver the first bytes. After this it's up to the client to request more.
    // TODO: only deliver the range requested in the original request
    for(size_t recipient_index = 0; recipient_index < wanted->recipients.length; recipient_index++)
    {
        ENetPeer *recipient = wanted->recipients.data[recipient_index];
        asset_user usr = { .server = server, .client = client, .peer = recipient };
        asset_deliver(asset_id, _asset_request_bytes_func, &usr);
    }

    for (size_t i = 0; i < sv->wanted_assets.length; i++) {
        wanted_asset *potential = sv->wanted_assets.data[i];
        if(potential == wanted)
        {
            arr_splice(&sv->wanted_assets, i, 1);
            free_wanted(wanted);
            break;
        }
    }
}

void _remove_client_from_wanted(alloserver *server, alloserver_client *client) {
    alloserv_internal *sv = _servinternal(server);
    ENetPeer *lost_peer = _clientinternal(client)->peer;
    
    server_log_asset(client, NULL, "Lost peer %p\n", lost_peer);

    for (size_t i = 0; i < sv->wanted_assets.length; i++) {
        wanted_asset *wanted = sv->wanted_assets.data[i];
        for(size_t recipient_index = 0; recipient_index < wanted->recipients.length; recipient_index++)
        {
            ENetPeer *peer = wanted->recipients.data[recipient_index];
            if(peer == lost_peer)
            {
                server_log_asset(client, wanted->id, "Lost a recipient peer");

                arr_splice(&wanted->recipients, recipient_index, 1);
                break;
            }
        }
        for(size_t source_index = 0; source_index < wanted->remaining_potential_sources.length; source_index++)
        {
            ENetPeer *source = wanted->remaining_potential_sources.data[source_index];
            if(source == lost_peer)
            {
                server_log_asset(client, wanted->id, "lost a potential source");
                arr_splice(&wanted->remaining_potential_sources, source_index, 1);
                break;
            }
        }
        if (wanted->remaining_potential_sources.length == 0) {
            server_log_asset(client, wanted->id, "nobody can deliver, let's fail this wanted asset.");
            _forward_wanted_asset_failure(wanted->id, server, client, wanted);
            // ^ will arr_splice wanted_assets, so need to decrease i
            --i;
        }
    }
}

const char *alloserv_describe_client(alloserver_client *client)
{
    static char desc[255];
    snprintf(desc, 255, "%s (ava %s/agent %s)", 
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(client->identity, "display_name")),
        client->avatar_entity_id, client->agent_id
    );
    return desc;
}
