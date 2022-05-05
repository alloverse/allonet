#define ALLO_INTERNALS 1
#include <allonet/state/diff.h>
#include <allonet/state/state_read.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alloverse_generated.h"
#include <flatbuffers/reflection.h>
#include "../alloverse_binary_schema.h"
using namespace Alloverse;

extern "C" void allo_state_diff_init(allo_state_diff *diff)
{
  arr_init(&diff->new_entities); arr_reserve(&diff->new_entities, 64);
  arr_init(&diff->deleted_entities); arr_reserve(&diff->deleted_entities, 64);
  arr_init(&diff->new_components); arr_reserve(&diff->new_components, 64);
  arr_init(&diff->updated_components); arr_reserve(&diff->updated_components, 64);
  arr_init(&diff->deleted_components); arr_reserve(&diff->deleted_components, 64);
}

static flatbuffers::voffset_t GetFieldOffsetInTable(const flatbuffers::Table *table, const reflection::Field *field)
{
    if(!table) return false;
    return table->GetOptionalFieldOffset(field->offset());
}

extern "C" void allo_state_diff_compute(allo_state_diff *diff, struct allo_state *oldstate, struct allo_state *newstate)
{
    auto schema = reflection::GetSchema(alloverse_schema_bytes);
    auto ComponentsTable = schema->objects()->LookupByKey("Components");

    // possible performance enhancement: use IterationVisitor in lockstep between the two buffers?

    for(auto newEntity : *newstate->_cur->entities())
    {
        auto eid = newEntity->id()->c_str();
        // check for added entities
        auto oldEntity = oldstate ? oldstate->_cur->entities()->LookupByKey(eid) : NULL;
        if(oldEntity == NULL)
        {
            arr_push(&diff->new_entities, eid);
        }

        // check for added/removed/changed comps
        auto oldComps = oldEntity ? oldEntity->components() : NULL;
        auto newComps = newEntity->components();

        // xxx: hack with hard-coded comps just to test
        #define TestComp(compname, classname) \
            classname##T compname##Old, compname##New; \
            if(oldComps && oldComps->compname()) oldComps->compname()->UnPackTo(&compname##Old);\
            if(newComps->compname()) newComps->compname()->UnPackTo(&compname##New); \
            if(!oldComps || (!oldComps->compname() && newComps->compname())) \
                allo_state_diff_mark_component_added(diff, eid, #compname, newComps->compname()); \
            else if(oldComps->compname() && !newComps->compname())\
                allo_state_diff_mark_component_deleted(diff, eid, #compname, oldComps->compname());\
            else if(oldComps->compname() && newComps->compname() && compname##Old != compname##New) \
                allo_state_diff_mark_component_updated(diff, eid, #compname, oldComps->compname(), newComps->compname());
        
        TestComp(transform, TransformComponent);
        TestComp(relationships, RelationshipsComponent);
        TestComp(live_media, LiveMediaComponent);
        TestComp(clock, ClockComponent);
        TestComp(intent, IntentComponent);
        TestComp(property_animations, PropertyAnimationsComponent);
        TestComp(geometry, GeometryComponent);
        TestComp(ui, UIComponent);
        TestComp(collider, ColliderComponent);
        TestComp(grabbable, GrabbableComponent);
        #undef TestComp

        // todo: using reflection to compare all comps, instead of doing it with macros like above
        /*for(auto ComponentField : *ComponentsTable->fields())
        {
            auto oldOffset = GetFieldOffsetInTable(oldComps, ComponentField);
            auto newOffset = GetFieldOffsetInTable(newComps, ComponentField);

        }*/

    }

    // check for removed entities
    if(oldstate) for(auto oldEntity : *oldstate->_cur->entities())
    {
        auto eid = oldEntity->id()->c_str();
        // check for added entities
        auto newEntity = newstate->_cur->entities()->LookupByKey(eid);
        if(!newEntity)
        {
            auto oldComps = oldEntity->components();
            arr_push(&diff->deleted_entities, eid);
            #define TestComp(compname, classname) \
                if(oldComps->compname())\
                    allo_state_diff_mark_component_deleted(diff, eid, #compname, oldComps->compname());
                    
            TestComp(transform, TransformComponent);
            TestComp(relationships, RelationshipsComponent);
            TestComp(live_media, LiveMediaComponent);
            TestComp(clock, ClockComponent);
            TestComp(intent, IntentComponent);
            TestComp(property_animations, PropertyAnimationsComponent);
            #undef TestComp
        }
    }
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
    to->data[i].olddata = (char*)from->data[i].olddata + pointer_delta;
    to->data[i].newdata = (char*)from->data[i].newdata + pointer_delta;
  }
}

extern "C" allo_state_diff *allo_state_diff_duplicate(allo_state_diff *orig, struct allo_state *oldstate, struct allo_state *newstate)
{
  ptrdiff_t pointer_delta = (char*)newstate->flat - (char*)oldstate->flat;
  allo_state_diff *diff = (allo_state_diff *)malloc(sizeof(*diff));

  _relocate_entity_ids(&orig->new_entities, &diff->new_entities, pointer_delta);
  _relocate_entity_ids(&orig->deleted_entities, &diff->deleted_entities, pointer_delta);
  _relocate_comp_refs(&orig->new_components, &diff->new_components, pointer_delta);
  _relocate_comp_refs(&orig->updated_components, &diff->updated_components, pointer_delta);
  _relocate_comp_refs(&orig->deleted_components, &diff->deleted_components, pointer_delta);

  return diff;
}

extern "C" void allo_state_diff_destroy(allo_state_diff *diff)
{
  arr_free(&diff->new_entities);
  arr_free(&diff->deleted_entities);
  arr_free(&diff->new_components);
  arr_free(&diff->updated_components);
  arr_free(&diff->deleted_components);
}
extern "C" void allo_state_diff_dump(allo_state_diff *diff)
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
void allo_state_diff_mark_component_added(allo_state_diff *diff, const char *eid, const char *cname, const void *comp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, NULL, comp};
  arr_push(&diff->new_components, ref);
}
extern void allo_state_diff_mark_component_updated(allo_state_diff *diff, const char *eid, const char *cname, const void *oldcomp, const void *newcomp)
{
  if(!diff) return;
  allo_component_ref ref = {eid, cname, oldcomp, newcomp};
  arr_push(&diff->updated_components, ref);
}
void allo_state_diff_mark_component_deleted(allo_state_diff *diff, const char *eid, const char *cname, const void *comp)
{
    if(!diff) return;
    allo_component_ref ref = {eid, cname, comp, NULL};
    arr_push(&diff->deleted_components, ref);
}
