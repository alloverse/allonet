//
//  util.h
//  allonet
//
//  Created by Nevyn Bengtsson on 1/2/19.
//

#ifndef util_h
#define util_h
#include <stdint.h>
#include <cJSON/cJSON.h>
#include <allonet/state.h>
#include <allonet/net.h>
#include <enet/enet.h>
#include <stdarg.h>
#include <stdio.h>

// return milliseconds since... some time ago
int64_t get_ts_mono(void);
double get_ts_monod(void);

uint64_t allo_create_random_seed(void);

allo_vector cjson2vec(const cJSON *veclist);
cJSON *vec2cjson(allo_vector vec);
allo_m4x4 cjson2m(const cJSON* matlist);
cJSON* m2cjson(allo_m4x4 mat);

int cjson_find_in_array(cJSON *array, const char *value);
void cjson_delete_from_array(cJSON *array, const char *value);
cJSON *cjson_create_object(const char *key, cJSON *value, ...);
cJSON *cjson_create_list(cJSON *value, ...);
int64_t cjson_get_int64_value(cJSON *value);
extern void cjson_clear(cJSON *object);


char *allo_strdup(const char *src); // null-safe
char *allo_strndup(const char *src, size_t n);
void allo_free(void *mallocd);


void _allo_media_initialize(void);



struct bitrate_data_t {
    size_t sent_bytes;
    size_t sent_packet_count;
    size_t received_bytes;
    size_t received_packet_count;
};

struct bitrate_t {
    struct bitrate_data_t data;
    struct bitrate_data_t snapshot;
    double shapshot_time;
};

typedef struct allo_statistics_t {
    unsigned int ndelta_set;
    unsigned int ndelta_merge;
    struct bitrate_t channel_rates[CHANNEL_COUNT+1];
} allo_statistics_t;
// because of threading without sync these values are just ballpark figures.
extern allo_statistics_t allo_statistics;


struct bitrate_deltas_t {
    double time;
    double sent_bytes;
    double received_bytes;
    
    size_t sent_packet_count;
    size_t received_packet_count;
};

static inline void bitrate_increment_sent(struct bitrate_t *br, size_t bytes_sent) {
    br->data.sent_bytes += bytes_sent;
    br->data.sent_packet_count ++;
}

static inline void bitrate_increment_received(struct bitrate_t *br, size_t bytes_received) {
    br->data.received_bytes += bytes_received;
    br->data.received_packet_count ++;
}

static inline struct bitrate_deltas_t bitrate_deltas(struct bitrate_t *br, double time) {
    struct bitrate_deltas_t delta;
    double dtime = time - br->shapshot_time;

    delta.time = dtime;
    delta.sent_bytes = (br->data.sent_bytes - br->snapshot.sent_bytes)/dtime;
    delta.received_bytes = (br->data.received_bytes - br->snapshot.received_bytes)/dtime;
    delta.sent_packet_count = (size_t)(br->data.sent_packet_count - br->snapshot.sent_packet_count)/dtime;
    delta.received_packet_count = (size_t)(br->data.received_packet_count - br->snapshot.received_packet_count)/dtime;
    if (dtime > 5 || dtime < 0) {
        br->snapshot = br->data;
        br->shapshot_time = time;
    }
    
    return delta;
}

static inline int allo_enet_peer_send(ENetPeer * peer, enet_uint8 channelID, ENetPacket * packet) {
    int result = enet_peer_send(peer, channelID, packet);
    if (result == 0) {
        bitrate_increment_sent(&allo_statistics.channel_rates[channelID], packet->dataLength);
        bitrate_increment_sent(&allo_statistics.channel_rates[CHANNEL_COUNT], packet->dataLength);
    } else {
        enet_packet_destroy(packet);
    }
    return result;
}

typedef enum LogType { ALLO_LOG_DEBUG, ALLO_LOG_INFO, ALLO_LOG_ERROR } LogType;
void allo_log(LogType type, const char *module, const char *identifiers, const char *format, ...);

//allo_log_func(asset);

#endif /* util_h */
