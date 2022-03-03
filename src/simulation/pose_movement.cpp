#define ALLO_INTERNALS 1
#include "simulation.h"
#include "alloverse_generated.h"
using namespace Alloverse;

void allosim_pose_movements(allo_state* state, Entity* avatar, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt, allo_state_diff *diff)
{
  (void)dt;
  auto entities = state->_cur->mutable_entities();

  for(int i = 0, c = entities->size(); i < c; i++)
  {
    Entity *entity = entities->GetMutableObject(i);
    auto rels = entity->components()->relationships();
    auto parent = rels ? rels->parent()->c_str() : NULL;
    auto intentc = entity->components()->intent();
    auto actuate_pose = intent ? intentc->actuate_pose()->c_str() : NULL;
    auto from_avatar = intent ? intentc->from_avatar() ? intentc->from_avatar()->c_str() : NULL : NULL;

    // don't care about entities that don't try to pose
    if (!actuate_pose)
      continue;
    // only do the work in this call of move_pose if this entity is owned by the current avatar
    if (entity->owner_agent_id() && strcmp(entity->owner_agent_id()->c_str(), avatar->owner_agent_id()->c_str()) != 0)
      continue;

    // if this entity wants to actuate some _other_ agent's intent...
    if (from_avatar)
    {
      for (int i = 0; i < intent_count; i++)
      {
        const allo_client_intent* other_intent = other_intents[i];
        if (strcmp(from_avatar, other_intent->entity_id) == 0) {
          intent = other_intent;
          break;
        }
      }
    }

    // if this is a client-side move pose, make sure we're only moving this client's avatar...
    if(!from_avatar)
      if(parent ? strcmp(intent->entity_id, parent) : strcmp(intent->entity_id, entity->id))
        continue;

    allo_m4x4 new_transform;
    if (strcmp(actuate_pose, "hand/left") == 0) new_transform = intent->poses.left_hand.matrix;
    else if (strcmp(actuate_pose, "hand/right") == 0) new_transform = intent->poses.right_hand.matrix;
    else if (strcmp(actuate_pose, "head") == 0) new_transform = intent->poses.head.matrix;
    else if (strcmp(actuate_pose, "torso") == 0) new_transform = intent->poses.torso.matrix;
    else if (strcmp(actuate_pose, "root") == 0) new_transform = intent->poses.root.matrix;
    else continue;

    // ignore identity transform, since it probably means nothing has been set
    if (allo_m4x4_is_identity(new_transform))
      continue;

    SetEntityTransform(entity, new_transform);
    allo_state_diff_mark_component_updated(diff, entity->id()->c_str(), "transform", NULL);
  }
}
