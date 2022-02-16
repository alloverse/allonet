#ifndef ALLONET_STATE_INTERACTION_H
#define ALLONET_STATE_INTERACTION_H
#include "math.h"
#include <stdint.h>
#include <stdbool.h>
#include <cJSON/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Describes an interaction to be sent or as received.
 * @field type: oneway, request, response or publication
 * @field sender_entity_id: the entity trying to interact with yours
 * @field receiver_entity_id: your entity being interacted with
 * @field request_id: The ID of this request or response
 * @field body: JSON list of interaction message
 */
typedef struct allo_interaction
{
    const char *type;
    const char *sender_entity_id;
    const char *receiver_entity_id;
    const char *request_id;
    const char *body;
} allo_interaction;

allo_interaction *allo_interaction_create(const char *type, const char *sender_entity_id, const char *receiver_entity_id, const char *request_id, const char *body);
extern cJSON* allo_interaction_to_cjson(const allo_interaction* interaction);
extern allo_interaction *allo_interaction_parse_cjson(const cJSON* from);
extern allo_interaction *allo_interaction_clone(const allo_interaction *interaction);
extern void allo_interaction_free(allo_interaction *interaction);


#ifdef __cplusplus
}
#endif
#endif
