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

// return milliseconds since... some time ago
int64_t get_ts_mono(void);
double get_ts_monod(void);

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



struct bitrate_t {
    double time;
    size_t send_count;
    size_t receive_count;
    size_t bps_sent;
    size_t bps_received;
    size_t count;
    size_t bps_count;
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
    double count;
    double sent;
    double received;
};

static inline void bitrate_increment(struct bitrate_t *br, size_t bytes_sent, size_t bytes_received) {
    br->send_count += bytes_sent;
    br->receive_count += bytes_received;
}
static inline struct bitrate_deltas_t bitrate_deltas(struct bitrate_t *br, double time) {
    struct bitrate_deltas_t delta;
    double dtime = time - br->time;

    delta.time = dtime;
    delta.count = 0;
    delta.sent = (br->send_count - br->bps_sent)/dtime/1024.0;
    delta.received = (br->send_count - br->bps_sent)/dtime/1024.0;
    
    if (dtime > 5 || dtime < 0) {
        br->bps_sent = br->send_count;
        br->bps_received = br->receive_count;
        br->time = time;
        br->bps_count = br->count;
    }
    
    return delta;
}

#endif /* util_h */
