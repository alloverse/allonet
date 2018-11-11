#include <allonet/state.h>
#include <string.h>

allo_entity *entity_create(const char *id)
{
    allo_entity *entity = calloc(1, sizeof(allo_entity));
    entity->id = strdup(id);
    return entity;
}
void entity_destroy(allo_entity *entity)
{
    free(entity->id);
    free(entity);
}