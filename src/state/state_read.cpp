#define ALLO_INTERNALS 1
#include <allonet/state/state_read.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "alloverse_generated.h"
using namespace Alloverse;

extern "C" void allo_state_create_parsed(allo_state *state, const void *buf, size_t len)
{
    state->flat = calloc(1, len);
    // copy over the data we need
    memcpy(state->flat, buf, len);
    state->flatlength = len;
    
    // parse a read-only version with the C API so API clients can use the state
    state->state = Alloverse_State_as_root(state->flat);
    state->revision = Alloverse_State_revision_get(state->state);

    // parse read-only with the C++ API so internal code can use easier APIs.
    state->_cur = GetMutableState(state->flat);
}

extern "C" allo_state *allo_state_duplicate(allo_state *state)
{
    allo_state *clone = (allo_state*)calloc(1, sizeof(*clone));
    allo_state_create_parsed(clone, state->flat, state->flatlength);
    return clone;
}

extern "C" void allo_state_destroy(allo_state *state)
{
    if(!state) return;

    free(state->flat);
}

extern "C" const allo_entity* state_get_entity(allo_state* state, const char* entity_id)
{
    Alloverse_Entity_vec_t vec = Alloverse_State_entities_get(state->state);
    size_t idx = Alloverse_Entity_vec_find_by_id(vec, entity_id);
    if(idx == flatbuffers_not_found)
    {
        return NULL;
    }
    return Alloverse_Entity_vec_at(vec, idx);
}

extern "C" const allo_entity* entity_get_parent(allo_state* state, const allo_entity* entity)
{
    auto comps = Alloverse_Entity_components_get(entity);
    auto rels = Alloverse_Components_relationships_get(comps);
    if(!rels) return NULL;
    auto parent = Alloverse_RelationshipsComponent_parent_get(rels);
    if(!parent) return NULL;
    return state_get_entity(state, parent);
}

extern allo_m4x4 entity_get_transform(const allo_entity* entity)
{
    return GetEntityTransform((Alloverse::Entity*)entity);
}

extern "C" double state_set_server_time(allo_state *state, double server_time)
{
    return state->setServerTime(server_time);
}

extern "C" allo_m4x4 entity_get_transform_in_coordinate_space(allo_state *state, allo_entity* entity, allo_entity* space)
{
    allo_m4x4 m = entity_get_transform(entity);
    return state_convert_coordinate_space(state, m, entity_get_parent(state, entity), space);
}

static allo_m4x4 entity_get_transform_to_world(allo_state* state, const allo_entity *ent)
{
    if (!ent) {
        return allo_m4x4_identity();
    }
    const allo_entity* parent = entity_get_parent(state, ent);
    allo_m4x4 my_transform = entity_get_transform(ent);
    if (parent) {
        return allo_m4x4_concat(entity_get_transform_to_world(state, parent), my_transform);
    }
    return my_transform;
}

extern "C" allo_m4x4 state_convert_coordinate_space(allo_state* state, allo_m4x4 m, const allo_entity* old, const allo_entity* neww)
{
    allo_m4x4 worldFromOld = entity_get_transform_to_world(state, old);
    allo_m4x4 worldFromNew = entity_get_transform_to_world(state, neww);
    allo_m4x4 newFromWorld = allo_m4x4_inverse(worldFromNew);
    allo_m4x4 newFromOld = allo_m4x4_concat(newFromWorld, worldFromOld);

    return allo_m4x4_concat(newFromOld, m);
}

Entity *
allo_state::getMutableEntity(const char *id)
{
    return _cur ? _cur->mutable_entities()->MutableLookupByKey(id) : NULL;
}

double
allo_state::setServerTime(double time)
{
    Entity *place = getMutableEntity("place");
    auto clock = place->mutable_components()->mutable_clock();
    double prev = clock->time();
    bool ok = clock->mutate_time(time);
    assert(ok); (void)ok;
    return prev;
}

allo_m4x4
GetEntityTransform(const Alloverse::Entity *ent)
{
    allo_m4x4 ret;
    auto transformcomp = ent->components()->transform();
    auto mat = transformcomp->matrix()->m();
    
    for(int i = 0; i < 16; i++)
        ret.v[i] = mat->Get(i);
    return ret;
}
void
SetEntityTransform(Entity *ent, allo_m4x4 transform)
{
    auto transformcomp = ent->mutable_components()->mutable_transform();
    auto mat = transformcomp->mutable_matrix()->mutable_m();
    for(int i = 0; i < 16; i++)
        mat->Mutate(i, transform.v[i]);
}
