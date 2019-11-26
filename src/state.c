#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

allo_m4x4 allo_m4x4_identity()
{
	return (allo_m4x4){
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}

allo_m4x4 allo_m4x4_translate(double x, double y, double z)
{
	return (allo_m4x4) {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1
	};
}
allo_m4x4 allo_m4x4_rotate(double rx, double ry, double rz);
allo_m4x4 allo_m4x4_concat(allo_m4x4 l, allo_m4x4 r);

allo_entity *entity_create(const char *id)
{
    allo_entity *entity = (allo_entity *)calloc(1, sizeof(allo_entity));
    entity->id = strdup(id);
    return entity;
}
void entity_destroy(allo_entity *entity)
{
    cJSON_Delete(entity->components);
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

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
        allo_interaction *interaction = (allo_interaction*)malloc(sizeof(allo_interaction));
        interaction->type = strdup(type);
        interaction->sender_entity_id = strdup(sender_entity_id);
        interaction->receiver_entity_id  = strdup(receiver_entity_id);
        interaction->request_id = strdup(request_id);
        interaction->body = strdup(body);
        return interaction;
}
void allo_interaction_free(allo_interaction *interaction)
{
    free((void*)interaction->type);
    free((void*)interaction->sender_entity_id);
    free((void*)interaction->receiver_entity_id);
    free((void*)interaction->request_id);
    free((void*)interaction->body);
    free(interaction);
}