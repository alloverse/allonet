#define ALLO_INTERNALS 1
#include <allonet/state/state_write.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

using namespace Alloverse;
using namespace std;
using namespace flatbuffers;

allo_mutable_state::allo_mutable_state()
{
    revision = 1;
    flatlength = 0;
    flat = NULL;
    _cur = NULL;
}

void
allo_mutable_state::finishIterationAndFlatten()
{
    // parse 'next' into 'cur' and 'flat' so it canonically becomes the "current"
    // read-only world state for this iteration.
    FlatBufferBuilder fbb;
    fbb.Finish(State::Pack(fbb, &next));
    allo_state_create_parsed(this, fbb.GetBufferPointer(), fbb.GetSize());

    // setup the revision number for the next iteration of the world state.
    int64_t next_revision = revision + 1;
    // roll over revision to 0 before it reaches biggest consecutive integer representable in json
    if(next_revision == 9007199254740990) { next_revision = 0; }
    next.revision = next_revision;
}

shared_ptr<EntityT> 
allo_mutable_state::getNextEntity(const char *id)
{
    
    /*shared_ptr<EntityT> ret;
        binary_search(next.entities.begin(), next.entities.end(), sid, 
        [&ret](shared_ptr<EntityT> const & ent, string &value) {
            if(ent->id == value) ret = ent;
            return ent->id < value;
        }
    );*/
    for(int i = 0, c = next.entities.size(); i < c; i++)
    {
        auto ent = next.entities[i];
        if(ent->id == id)
            return ent;
    }
    return NULL;
}

static bool eid_pred(const EntityT &l, const EntityT &r)
{
    return l.id < r.id;
}

shared_ptr<EntityT> 
allo_mutable_state::addEntity(const char *id)
{
    shared_ptr<EntityT> entity = make_shared<EntityT>();
    
    // required defaults for all entities
    entity->id = id;
    entity->components = make_shared<ComponentsT>();
    entity->components->transform = make_shared<TransformComponentT>();
    entity->components->transform->matrix = make_shared<Mat4>(flatbuffers::make_span(array<double, 16>{1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1}));
  
    /*next.entities.insert( 
        std::upper_bound(
            next.entities.begin(), next.entities.end(), 
            entity, eid_pred
        ),
        entity
    );*/
    next.entities.push_back(entity);
    return entity;
}

shared_ptr<EntityT> 
allo_mutable_state::addEntityFromSpec(shared_ptr<EntitySpecT> spec, const char *agentId, const char *parent)
{
    char generated_eid[11] = { 0 };
    allo_generate_id(generated_eid, 11);
    auto root = addEntity(generated_eid);
    root->owner_agent_id = agentId ? agentId : "place";
    root->components = spec->components;

    if(parent)
    {
        if(!root->components->relationships)
        {
            root->components->relationships = make_shared<RelationshipsComponentT>();
        }
        root->components->relationships->parent = parent;
    }

    for(auto subspec: spec->children)
    {
        addEntityFromSpec(subspec, agentId, generated_eid);
    }

    return root;
}

static bool has_value(vector<string> vec, string val)
{
    return find(vec.begin(), vec.end(), val) != vec.end();
}

void
allo_mutable_state::changeComponents(shared_ptr<EntityT> entity, shared_ptr<ComponentsT> addChange, std::vector<std::string> remove)
{
    // XXX: This is the saddest code in the flatbuffer rewrite :( Would love to make this more generic,
    // but can't think of a good + generic way.
    if(addChange->transform || has_value(remove, "transform")) entity->components->transform = addChange->transform;
    if(addChange->relationships || has_value(remove, "relationships")) entity->components->relationships = addChange->relationships;
    if(addChange->live_media || has_value(remove, "live_media")) entity->components->live_media = addChange->live_media;
    if(addChange->clock || has_value(remove, "clock")) entity->components->clock = addChange->clock;
    if(addChange->intent || has_value(remove, "intent")) entity->components->intent = addChange->intent;
    if(addChange->property_animations || has_value(remove, "property_animations")) entity->components->property_animations = addChange->property_animations;
    if(addChange->geometry || has_value(remove, "geometry")) entity->components->geometry = addChange->geometry;
    if(addChange->ui || has_value(remove, "ui")) entity->components->ui = addChange->ui;
    if(addChange->collider || has_value(remove, "collider")) entity->components->collider = addChange->collider;
    if(addChange->grabbable || has_value(remove, "grabbable")) entity->components->grabbable = addChange->grabbable;
}





double
allo_mutable_state::setServerTime(double time)
{
  auto clock = getNextEntity("place")->components.get()->clock.get();
  auto prev = clock->time;
  clock->time = time;
  return prev;
}

bool 
allo_mutable_state::removeEntity(allo_removal_mode mode, const char *id)
{
    auto it = find_if(next.entities.begin(), next.entities.end(),
        [&id](const shared_ptr<EntityT> &ent) {
            return ent->id == id;
        }
    );
    next.entities.erase(it);

    // todo: remove children
    
    return false;
}

void
allo_mutable_state::removeEntitiesForAgent(const char *agent_id)
{
    for(size_t i = next.entities.size(); i-- > 0; )
    {
        auto ent = next.entities[i];
        if(ent->owner_agent_id == agent_id)
        {
            removeEntity(AlloRemovalCascade, ent->id.c_str());
        }
    }
}

