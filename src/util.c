//
//  util.c
//  allonet
//
//  Created by Nevyn Bengtsson on 1/2/19.
//

#include "util.h"
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#endif

int64_t
get_ts_mono(void)
{
#ifdef _WIN32
  return GetTickCount64();
#else
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return (int64_t)tv.tv_sec * 1000000LL + (tv.tv_nsec / 1000);
#endif
}

allo_vector cjson2vec(const cJSON *veclist)
{
    const cJSON *x = cJSON_GetArrayItem(veclist, 0);
    const cJSON *y = cJSON_GetArrayItem(veclist, 1);
    const cJSON *z = cJSON_GetArrayItem(veclist, 2);
    return (allo_vector){x->valuedouble, y->valuedouble, z->valuedouble};
}

cJSON *vec2cjson(allo_vector vec)
{
    return cjson_create_object(
        "x", cJSON_CreateNumber(vec.x),
        "y", cJSON_CreateNumber(vec.y),
        "z", cJSON_CreateNumber(vec.z),
        NULL
    );
}

cJSON* m2cjson(allo_m4x4 mat)
{
	return cjson_create_list(
		cJSON_CreateNumber(mat.c1r1), cJSON_CreateNumber(mat.c1r2), cJSON_CreateNumber(mat.c1r3), cJSON_CreateNumber(mat.c1r4),
		cJSON_CreateNumber(mat.c2r1), cJSON_CreateNumber(mat.c2r2), cJSON_CreateNumber(mat.c2r3), cJSON_CreateNumber(mat.c2r4),
		cJSON_CreateNumber(mat.c3r1), cJSON_CreateNumber(mat.c3r2), cJSON_CreateNumber(mat.c3r3), cJSON_CreateNumber(mat.c3r4),
		cJSON_CreateNumber(mat.c4r1), cJSON_CreateNumber(mat.c4r2), cJSON_CreateNumber(mat.c4r3), cJSON_CreateNumber(mat.c4r4),
		NULL
	);
}

allo_m4x4 cjson2m(const cJSON* matlist)
{
  allo_m4x4 m;
  for (int i = 0; i < 16; i++) {
    m.v[i] = cJSON_GetArrayItem(matlist, i)->valuedouble;
  }
  return m;
}

int cjson_find_in_array(cJSON *array, const char *value)
{
    int i = 0;
    const cJSON *child = NULL;
    cJSON_ArrayForEach(child, array) {
        if(strcmp(child->valuestring, value) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}
void cjson_delete_from_array(cJSON *array, const char *value)
{
    int index = cjson_find_in_array(array, value);
    if(index != -1) {
        cJSON_DeleteItemFromArray(array, index);
    }
}

cJSON *cjson_create_object(const char *key, cJSON *value, ...)
{
    cJSON *parent = cJSON_CreateObject();
    va_list args;
    va_start(args, value);
    while(key != NULL && value != NULL) {
        cJSON_AddItemToObject(parent, key, value);
        
        key = va_arg(args, const char *);
        if(key) {
            value = va_arg(args, cJSON *);
        }
    }
    va_end(args);
    return parent;
}

cJSON *cjson_create_list(cJSON *value, ...)
{
    cJSON *parent = cJSON_CreateArray();
    va_list args;
    va_start(args, value);
    while(value != NULL) {
        cJSON_AddItemToArray(parent, value);
        value = va_arg(args, cJSON *);
    }
    va_end(args);
    return parent;
}

char *allo_strndup(const char *src, size_t n)
{
    char *dest = calloc(1, n+1);
    for(size_t i = 0; i < n; i++) {
        dest[i] = src[i];
    }
    return dest;
}
