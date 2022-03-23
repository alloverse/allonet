#include <allonet/state/diff.h>
#include <allonet/state/state_read.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void allo_state_diff_init(allo_state_diff *diff)
{
  arr_init(&diff->new_entities); arr_reserve(&diff->new_entities, 64);
  arr_init(&diff->deleted_entities); arr_reserve(&diff->deleted_entities, 64);
  arr_init(&diff->new_components); arr_reserve(&diff->new_components, 64);
  arr_init(&diff->updated_components); arr_reserve(&diff->updated_components, 64);
  arr_init(&diff->deleted_components); arr_reserve(&diff->deleted_components, 64);
}

static void _relocate_entity_ids(allo_entity_id_vec *from, allo_entity_id_vec *to, ptrdiff_t pointer_delta)
{
  arr_init(to);
  arr_reserve(to, from->capacity);
  for(int i = 0, c = from->length; i < c; i++)
  {
    to->data[i] = from->data[i] + pointer_delta;
  }
}
static void _relocate_comp_refs(allo_component_vec *from, allo_component_vec *to, ptrdiff_t pointer_delta)
{
  arr_init(to);
  arr_reserve(to, from->capacity);
  for(int i = 0, c = from->length; i < c; i++)
  {
    to->data[i].eid = from->data[i].eid + pointer_delta;
    to->data[i].name = from->data[i].name + pointer_delta;
    to->data[i].olddata = from->data[i].olddata + pointer_delta;
    to->data[i].newdata = from->data[i].newdata + pointer_delta;
  }
}

allo_state_diff *allo_state_diff_duplicate(allo_state_diff *orig, struct allo_state *oldstate, struct allo_state *newstate)
{
  ptrdiff_t pointer_delta = newstate->flat - oldstate->flat;
  allo_state_diff *diff = malloc(sizeof(*diff));

  _relocate_entity_ids(&orig->new_entities, &diff->new_entities, pointer_delta);
  _relocate_entity_ids(&orig->deleted_entities, &diff->deleted_entities, pointer_delta);
  _relocate_comp_refs(&orig->new_components, &diff->new_components, pointer_delta);
  _relocate_comp_refs(&orig->updated_components, &diff->updated_components, pointer_delta);
  _relocate_comp_refs(&orig->deleted_components, &diff->deleted_components, pointer_delta);

  return diff;
}

void allo_state_diff_destroy(allo_state_diff *diff)
{
  arr_free(&diff->new_entities);
  arr_free(&diff->deleted_entities);
  arr_free(&diff->new_components);
  arr_free(&diff->updated_components);
  arr_free(&diff->deleted_components);
}
void allo_state_diff_dump(allo_state_diff *diff)
{
  printf("=============== Statediff ================\n");
  for(size_t i = 0; i < diff->new_entities.length; i++)
  {
    printf("New entity: %s\n", diff->new_entities.data[i]);
  }
  for(size_t i = 0; i < diff->deleted_entities.length; i++)
  {
    printf("Deleted entity: %s\n", diff->deleted_entities.data[i]);
  }
  for(size_t i = 0; i < diff->new_components.length; i++)
  {
    printf("New component: %s.%s\n", diff->new_components.data[i].eid, diff->new_components.data[i].name);
  }
  for(size_t i = 0; i < diff->updated_components.length; i++)
  {
    printf("Updated component: %s.%s\n", diff->updated_components.data[i].eid, diff->updated_components.data[i].name);
  }
  for(size_t i = 0; i < diff->deleted_components.length; i++)
  {
    printf("Deleted component: %s.%s\n", diff->deleted_components.data[i].eid, diff->deleted_components.data[i].name);
  }
}
void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->new_components, ref);
}
void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->updated_components, ref);
}
