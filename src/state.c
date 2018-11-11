#include <allonet/state.h>
#include <string.h>
#include <stdlib.h>

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