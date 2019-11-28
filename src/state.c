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

allo_m4x4 allo_m4x4_translate(allo_vector t)
{
	return (allo_m4x4) {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		t.x, t.y, t.z, 1
	};
}

allo_m4x4 allo_m4x4_rotate(double phi, allo_vector axis)
{
	// http://www.opengl-tutorial.org/assets/faq_quaternions/index.html#Q26
	allo_m4x4 m = allo_m4x4_identity();
	double rcos = cos(phi);
	double rsin = sin(phi);
	double u = axis.x, v = axis.y, w = axis.z;
	m.m11 = rcos + u * u * (1 - rcos);
	m.m12 = w * rsin + v * u * (1 - rcos);
	m.m13 = -v * rsin + w * u * (1 - rcos);
	m.m21 = -w * rsin + u * v * (1 - rcos);
	m.m22 = rcos + v * v * (1 - rcos);
	m.m23 = u * rsin + w * v * (1 - rcos);
	m.m31 = v * rsin + u * w * (1 - rcos);
	m.m32 = -u * rsin + v * w * (1 - rcos);
	m.m33 = rcos + w * w * (1 - rcos);

	return m;
}

#define rc(m, r, c) m.v[r*4 + c]

allo_m4x4 allo_m4x4_concat(allo_m4x4 a, allo_m4x4 b)
{
	allo_m4x4 m = allo_m4x4_identity();
	for (int r = 0; r <= 3; r++) {
		for (int c = 0; c <= 3; c++) {
			rc(m, r, c) =
				rc(a, r, 0) * rc(b, 0, c) +
				rc(a, r, 1) * rc(b, 1, c) +
				rc(a, r, 2) * rc(b, 2, c) +
				rc(a, r, 3) * rc(b, 3, c);
		}
	}
	return m;
}

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