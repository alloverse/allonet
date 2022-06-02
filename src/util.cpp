//
//  util.c
//  allonet
//
//  Created by Nevyn Bengtsson on 1/2/19.
//

extern "C" {
#include "util.h"
}
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#ifdef _WIN32
    #include <process.h>
#else
    #include <sys/types.h>
    #include <unistd.h>
#endif
#include <allonet/threading.h>


allo_statistics_t allo_statistics = {0};

extern "C" int64_t
get_ts_mono(void)
{
#ifdef _WIN32
  return GetTickCount64();
#else
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return (int64_t)tv.tv_sec * 1000LL + (tv.tv_nsec / 1000000);
#endif
}

extern "C" double get_ts_monod(void)
{
    return get_ts_mono()/1000.0;
}

extern "C" uint64_t allo_create_random_seed(void)
{
    uint64_t t = get_ts_mono();
    t += getpid();
    t += (uint64_t)thrd_current();
    t = (t << 16) | (t >> 16);
    return t;
}


extern "C" allo_vector cjson2vec(const cJSON *veclist)
{
    const cJSON *x = cJSON_GetArrayItem(veclist, 0);
    const cJSON *y = cJSON_GetArrayItem(veclist, 1);
    const cJSON *z = cJSON_GetArrayItem(veclist, 2);
    if (!x || !y || !z)
    {
        allo_vector v;
        v.v[0] = 0.0;
        v.v[1] = 0.0;
        v.v[2] = 0.0;
        return v;
    }
    allo_vector v;
    v.v[0] = x->valuedouble;
    v.v[1] = y->valuedouble;
    v.v[2] = z->valuedouble;
    return v;
}

extern "C" cJSON *vec2cjson(allo_vector vec)
{
    return cjson_create_list(
        cJSON_CreateNumber(vec.x),
        cJSON_CreateNumber(vec.y),
        cJSON_CreateNumber(vec.z),
        NULL
    );
}

extern "C" cJSON* m2cjson(allo_m4x4 mat)
{
	return cjson_create_list(
		cJSON_CreateNumber(mat.c1r1), cJSON_CreateNumber(mat.c1r2), cJSON_CreateNumber(mat.c1r3), cJSON_CreateNumber(mat.c1r4),
		cJSON_CreateNumber(mat.c2r1), cJSON_CreateNumber(mat.c2r2), cJSON_CreateNumber(mat.c2r3), cJSON_CreateNumber(mat.c2r4),
		cJSON_CreateNumber(mat.c3r1), cJSON_CreateNumber(mat.c3r2), cJSON_CreateNumber(mat.c3r3), cJSON_CreateNumber(mat.c3r4),
		cJSON_CreateNumber(mat.c4r1), cJSON_CreateNumber(mat.c4r2), cJSON_CreateNumber(mat.c4r3), cJSON_CreateNumber(mat.c4r4),
		NULL
	);
}

extern "C" allo_m4x4 cjson2m(const cJSON* matlist)
{
  if (matlist == NULL || cJSON_GetArraySize(matlist) != 16)
    return allo_m4x4_identity();
  allo_m4x4 m;
  for (int i = 0; i < 16; i++) {
    m.v[i] = cJSON_GetArrayItem(matlist, i)->valuedouble;
  }
  return m;
}

extern "C" int cjson_find_in_array(cJSON *array, const char *value)
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
extern "C" void cjson_delete_from_array(cJSON *array, const char *value)
{
    int index = cjson_find_in_array(array, value);
    if(index != -1) {
        cJSON_DeleteItemFromArray(array, index);
    }
}

extern "C" cJSON *cjson_create_object(const char *key, cJSON *value, ...)
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

extern "C" cJSON *cjson_create_list(cJSON *value, ...)
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

extern "C" int64_t cjson_get_int64_value(cJSON *value)
{
    if(value == NULL)
    {
        return 0;
    }
    return value->valuedouble;
}

extern "C" void cjson_clear(cJSON *object)
{
    cJSON *child = object->child;
    while(child) {
        cJSON *next= child->next;
        cJSON_DetachItemViaPointer(object, child);
        cJSON_Delete(child);
        child = next;
    }
}

extern "C" char *allo_strdup(const char *src)
{
    if(src == NULL)
    {
        return NULL;
    }
    return strdup(src);
}

extern "C" char *allo_strndup(const char *src, size_t n)
{
    if(src == NULL || n == 0) 
    {
        return NULL;
    }
    char *dest = (char*)calloc(1, n+1);
    for(size_t i = 0; i < n; i++) {
        dest[i] = src[i];
    }
    return dest;
}

extern "C" void allo_free(void *mallocd)
{
    free(mallocd);
}


static char *LogTypeNames[3] = {
    "DEBUG", "INFO", "ERROR"
};


#if defined(__WINDOWS__)
extern "C" void allo_log(LogType type, const char *module, const char *identifiers, const char *format, ...) {
    FILE *whereto = stdout;
    if (type == ALLO_LOG_ERROR) whereto = stderr;
    va_list args;
    va_start(args, format);
    vfprintf(whereto, format, args);
    va_end(args);
}
#else
extern "C" void allo_log(LogType type, const char *module, const char *identifiers, const char *format, ...) {
    // TODO: filter out DEBUG types if not building debug, but apparently cmake is not adding any flag?
    char *message;
    va_list args;
    va_start(args, format);
    int ret = vasprintf(&message, format, args);
    va_end(args);
    FILE *whereto = stdout;
    if (type == ALLO_LOG_ERROR) whereto = stderr;
    fprintf(whereto, "[%s] %s %s \"%s\"\n", LogTypeNames[type], module, identifiers, message);
    if (ret != -1)
        free(message);
}
#endif
