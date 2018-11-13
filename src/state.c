#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

allo_entity *entity_create(const char *id)
{
    allo_entity *entity = (allo_entity *)calloc(1, sizeof(allo_entity));
    entity->id = strdup(id);
    return entity;
}
void entity_destroy(allo_entity *entity)
{
    free(entity->id);
    free(entity);
}

// move to allonet.c
#include <enet/enet.h>
extern bool allo_initialize(bool redirect_stdout)
{
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
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        return false;
    }
    atexit (enet_deinitialize);

    return true;
}