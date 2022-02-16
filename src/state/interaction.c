#include <allonet/state/interaction.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body)
{
    allo_interaction *interaction = (allo_interaction*)malloc(sizeof(allo_interaction));
    interaction->type = allo_strdup(type);
    interaction->sender_entity_id = allo_strdup(sender_entity_id);
    interaction->receiver_entity_id = allo_strdup(receiver_entity_id);
    interaction->request_id = allo_strdup(request_id);
    interaction->body = allo_strdup(body);
    return interaction;
}

void allo_interaction_free(allo_interaction *interaction)
{
    free((void*)interaction->type);
    free((void*)interaction->sender_entity_id);
    free((void*)interaction->receiver_entity_id);
    free((void*)interaction->request_id);
    free((void*)interaction->body);
    free(interaction);
}

allo_interaction *allo_interaction_clone(const allo_interaction *interaction)
{
  return allo_interaction_create(interaction->type, interaction->sender_entity_id, interaction->receiver_entity_id, interaction->request_id, interaction->body);
}

cJSON* allo_interaction_to_cjson(const allo_interaction* interaction)
{
  return cjson_create_list(
    cJSON_CreateString("interaction"),
    cJSON_CreateString(interaction->type),
    cJSON_CreateString(interaction->sender_entity_id),
    cJSON_CreateString(interaction->receiver_entity_id),
    cJSON_CreateString(interaction->request_id),
    cJSON_Parse(interaction->body),
    NULL
  );
}
allo_interaction *allo_interaction_parse_cjson(const cJSON* inter_json)
{
  const char* type = cJSON_GetArrayItem(inter_json, 1)->valuestring;
  const char* from = cJSON_GetArrayItem(inter_json, 2)->valuestring;
  const char* to = cJSON_GetArrayItem(inter_json, 3)->valuestring;
  const char* request_id = cJSON_GetArrayItem(inter_json, 4)->valuestring;
  cJSON* body = cJSON_GetArrayItem(inter_json, 5);
  const char* bodystr = cJSON_Print(body);
  allo_interaction *interaction = allo_interaction_create(type, from, to, request_id, bodystr);
  free((void*)bodystr);
  return interaction;
}
