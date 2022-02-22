#ifndef ALLONET_STATE_WRITE_H
#define ALLONET_STATE_WRITE_H

#include <allonet/state/state_read.h>

#define ALLO_INTERNALS 1
#if defined(__cplusplus) && defined(ALLO_INTERNALS)
#include "alloverse_generated.h"
#include <vector>
#include <string>

typedef enum allo_removal_mode
{
    AlloRemovalCascade,
    AlloRemovalReparent,
} allo_removal_mode;

struct allo_mutable_state : allo_state
{
public:
    allo_mutable_state();
    // internal mutable object tree, only valid on server
    Alloverse::StateT next;

    // transform next into cur/flat and bump revision so it can be transmitted.
    void finishIteration();

    Alloverse::EntityT *addEntity(const char *id);
    Alloverse::EntityT *addEntityFromSpec(const char *spec, const char *agentId, const char *parent);

    bool removeEntity(allo_removal_mode mode, const char *id);
    void removeEntitiesForAgent(const char *agent_id);

    Alloverse::EntityT *getEntity(const char *id);
    void changeComponents(Alloverse::EntityT *entity, const char *addChange, std::vector<std::string> remove);
};

#endif // _cplusplus && ALLO_INTERNALS
#endif // ALLONET_STATE_WRITE_H
