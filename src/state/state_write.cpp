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
    entity->id = id;
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
  
}

