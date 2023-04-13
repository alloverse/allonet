#include "simulation.h"

void allosim_pose_movements(allo_state* state, allo_entity* avatar, const allo_client_intent *intent, const allo_client_intent** other_intents, int intent_count, double dt, allo_state_diff *diff)
{
  (void)dt;
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* rels = cJSON_GetObjectItemCaseSensitive(entity->components, "relationships");
    const char* parent = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(rels, "parent"));
    cJSON* intents = cJSON_GetObjectItemCaseSensitive(entity->components, "intent");
    const char* actuate_pose = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(intents, "actuate_pose"));
    const char* from_avatar = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(intents, "from_avatar"));

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
    if(!from_avatar)
      if((parent && strcmp(actuate_pose, "root") != 0) ? strcmp(intent->entity_id, parent) : strcmp(intent->entity_id, entity->id))
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

    entity_set_transform(entity, new_transform);
    allo_state_diff_mark_component_updated(diff, entity->id, "transform", cJSON_GetObjectItemCaseSensitive(entity->components, "transform"));
  }
}
