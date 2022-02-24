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

EntityT *
allo_mutable_state::getNextEntity(const char *id)
{
  for(int i = 0, c = next.entities.size(); i < c; i++)
  {
    EntityT *ent = next.entities[i].get();
    if(ent->id == id)
      return ent;
  }
  return NULL;
}

double
allo_mutable_state::setServerTime(double time)
{
  auto clock = getNextEntity("place")->components->clock;
  auto prev = clock->time;
  clock->time = time;
  return prev;
}

extern "C" void allo_generate_id(char *str, size_t len)
{
  for (size_t i = 0; i < len-1; i++)
  {
    str[i] = 'a' + rand() % 25;
  }
  str[len-1] = 0;
}
