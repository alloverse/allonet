#include "simulation.h"

void allosim_pose_movement(allo_state* state, allo_entity* avatar, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt)
{
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* rels = cJSON_GetObjectItem(entity->components, "relationships");
    const char* parent = cJSON_GetStringValue(cJSON_GetObjectItem(rels, "parent"));
    cJSON* intents = cJSON_GetObjectItem(entity->components, "intent");
    const char* actuate_pose = cJSON_GetStringValue(cJSON_GetObjectItem(intents, "actuate_pose"));
    const char* from_avatar = cJSON_GetStringValue(cJSON_GetObjectItem(intents, "from_avatar"));

    // don't care about entities that don't try to pose
    if (!actuate_pose)
      continue;
    // only do the work in this call of move_pose if this entity is owned by the current avatar
    if (entity->owner_agent_id && strcmp(entity->owner_agent_id, avatar->owner_agent_id) != 0)
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
    if(!from_avatar && strcmp(intent->entity_id, parent))
      continue;

    allo_m4x4 new_transform;
    if (strcmp(actuate_pose, "hand/left") == 0) new_transform = intent->poses.left_hand.matrix;
    else if (strcmp(actuate_pose, "hand/right") == 0) new_transform = intent->poses.right_hand.matrix;
    else if (strcmp(actuate_pose, "head") == 0) new_transform = intent->poses.head.matrix;
    else continue;

    // ignore identity transform, since it probably means nothing has been set
    if (allo_m4x4_equal(new_transform, allo_m4x4_identity(), 0.00001))
      continue;

    entity_set_transform(entity, new_transform);
  }
}