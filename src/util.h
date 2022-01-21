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
    size_t sent_bytes;
    size_t received_bytes;
    size_t sample_count;
    
    double shapshot_time;
    size_t snapshot_sent_bytes;
    size_t shapshot_received_bytes;
    size_t snapshot_sample_count;
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
    double sample_count;
    double sent_bytes;
    double received_bytes;
};

static inline void bitrate_increment(struct bitrate_t *br, size_t bytes_sent, size_t bytes_received) {
    br->sent_bytes += bytes_sent;
    br->received_bytes += bytes_received;
}
static inline struct bitrate_deltas_t bitrate_deltas(struct bitrate_t *br, double time) {
    struct bitrate_deltas_t delta;
    double dtime = time - br->shapshot_time;

    delta.time = dtime;
    delta.sample_count = 0;
    delta.sent_bytes = (br->sent_bytes - br->snapshot_sent_bytes)/dtime;
    delta.received_bytes = (br->received_bytes - br->shapshot_received_bytes)/dtime;
    
    if (dtime > 5 || dtime < 0) {
        br->snapshot_sent_bytes = br->sent_bytes;
        br->shapshot_received_bytes = br->received_bytes;
        br->shapshot_time = time;
        br->snapshot_sample_count = br->sample_count;
    }
    
    return delta;
}

#endif /* util_h */
