//
//  util.c
//  allonet
//
//  Created by Nevyn Bengtsson on 1/2/19.
//

#include "util.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>

int64_t
get_ts_mono(void)
{
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return (int64_t)tv.tv_sec * 1000000LL + (tv.tv_nsec / 1000);
}

allo_vector cjson2vec(const cJSON *veclist)
{
    const cJSON *x = cJSON_GetArrayItem(veclist, 0);
    const cJSON *y = cJSON_GetArrayItem(veclist, 1);
    const cJSON *z = cJSON_GetArrayItem(veclist, 2);
    return (allo_vector){x->valuedouble, y->valuedouble, z->valuedouble};
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
