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

typedef struct allo_statistics_t {
    unsigned int ndelta_set;
    unsigned int ndelta_merge;
    unsigned int bytes_sent[6];//total + per channel
    unsigned int bytes_recv[6];//total + per channel
} allo_statistics_t;
// because of threading without sync these values are just ballpark figures.
extern allo_statistics_t allo_statistics;

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




#endif /* util_h */
