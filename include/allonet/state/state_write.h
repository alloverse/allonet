#ifndef ALLONET_STATE_WRITE_H
#define ALLONET_STATE_WRITE_H

#include <allonet/state/state_read.h>

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
    void finishIterationAndFlatten();

    Alloverse::EntityT *addEntity(const char *id);
    Alloverse::EntityT *addEntityFromSpec(const char *spec, const char *agentId, const char *parent);

    bool removeEntity(allo_removal_mode mode, const char *id);
    void removeEntitiesForAgent(const char *agent_id);

    Alloverse::EntityT *getNextEntity(const char *id);
    void changeComponents(Alloverse::EntityT *entity, const char *addChange, std::vector<std::string> remove);

    virtual double setServerTime(double time) override;
};

// generate an identifier of 'len'-1 chars into str, and null the last byte in str.
extern "C" void allo_generate_id(char *str, size_t len);


#endif // _cplusplus && ALLO_INTERNALS
#endif // ALLONET_STATE_WRITE_H
