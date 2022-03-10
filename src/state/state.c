#include <allonet/state.h>
#include <allonet/schema/reflection_reader.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../util.h"
#include <allonet/arr.h>
#include <assert.h>
#include <math.h>
#include "../alloverse_binary_schema.h"

static reflection_Schema_table_t g_alloschema;
static reflection_Object_vec_t SchemaTables;

extern "C" void allo_generate_id(char *str, size_t len)
{
  for (size_t i = 0; i < len-1; i++)
  {
    str[i] = 'a' + rand() % 25;
  }
  str[len-1] = 0;
}

// move to allonet.c
#include <enet/enet.h>
static bool _has_initialized = false;
bool allo_initialize(bool redirect_stdout)
{
    if (_has_initialized) return true;
    _has_initialized = true;

    setvbuf(stdout, NULL, _IONBF, 0);
    if(redirect_stdout) {
        printf("Moving stdout...\n");
        fflush(stdout);
        freopen("/tmp/debug.txt", "a", stdout);
        setvbuf(stdout, NULL, _IONBF, 0);
        printf("Stdout moved\n");
        fflush(stdout);
    }
    if (enet_initialize () != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return false;
    }
    atexit (enet_deinitialize);

    _allo_media_initialize();

    g_alloschema = reflection_Schema_as_root(alloverse_schema_bytes);
    assert(g_alloschema);
    if(!g_alloschema)
    {
        fprintf(stderr, "Allonet was unable to parse its flatbuffer schema.\n");
        return false;
    }
    SchemaTables = reflection_Schema_objects(g_alloschema);

    return true;
}

