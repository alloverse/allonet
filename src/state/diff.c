#include <allonet/state/diff.h>
#include <stdlib.h>
#include <stdio.h>

extern void allo_state_diff_init(allo_state_diff *diff)
{
  arr_init(&diff->new_entities); arr_reserve(&diff->new_entities, 64);
  arr_init(&diff->deleted_entities); arr_reserve(&diff->deleted_entities, 64);
  arr_init(&diff->new_components); arr_reserve(&diff->new_components, 64);
  arr_init(&diff->updated_components); arr_reserve(&diff->updated_components, 64);
  arr_init(&diff->deleted_components); arr_reserve(&diff->deleted_components, 64);
}
extern void allo_state_diff_destroy(allo_state_diff *diff)
{
  arr_free(&diff->new_entities);
  arr_free(&diff->deleted_entities);
  arr_free(&diff->new_components);
  arr_free(&diff->updated_components);
  arr_free(&diff->deleted_components);
}
extern void allo_state_diff_dump(allo_state_diff *diff)
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
extern void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->new_components, ref);
}
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const cJSON *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->updated_components, ref);
}
