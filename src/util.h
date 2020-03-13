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

int64_t get_ts_mono(void);

allo_vector cjson2vec(const cJSON *veclist);
cJSON *vec2cjson(allo_vector vec);
allo_m4x4 cjson2m(const cJSON* matlist);
cJSON* m2cjson(allo_m4x4 mat);

int cjson_find_in_array(cJSON *array, const char *value);
void cjson_delete_from_array(cJSON *array, const char *value);
cJSON *cjson_create_object(const char *key, cJSON *value, ...);
cJSON *cjson_create_list(cJSON *value, ...);

char *allo_strndup(const char *src, size_t n);



#endif /* util_h */
