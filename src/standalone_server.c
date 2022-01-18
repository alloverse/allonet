#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>
#include <errno.h>

#include <allonet/allonet.h>
#include "media/media.h"
#include "util.h"
#include "delta.h"

static alloserver* serv;
static allo_entity* place;
static double last_simulate_at = 0;
static char *g_placename;
static allo_media_track_list mediatracks;

static void send_interaction_to_client(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  cJSON* cmdrep = allo_interaction_to_cjson(interaction);
  const char* json = cJSON_Print(cmdrep);
  cJSON_Delete(cmdrep);

  serv->send(serv, client, CHANNEL_COMMANDS, (const uint8_t*)json, strlen(json));
  free((void*)json);
}

static void clients_changed(alloserver* serv, alloserver_client* added, alloserver_client* removed)
{
    (void)added;
    if (removed) {
        allo_entity* entity = serv->state.entities.lh_first;
        while (entity) {
            allo_entity* to_delete = entity;
            entity = entity->pointers.le_next;
            if (strcmp(to_delete->owner_agent_id, removed->agent_id) == 0) {
                LIST_REMOVE(to_delete, pointers);
                allo_state_remove_entity(&serv->state, to_delete, AlloRemovalCascade);
            }
        }
        
        for (size_t i = 0; i < mediatracks.length; i++) {
            /// Remove the client from any track recipient lists
            allo_media_track *track = &mediatracks.data[i];
            for (size_t j = 0; j < track->recipients.length; j++) {
                if (track->recipients.data[j] == removed) {
                    arr_splice(&track->recipients, j, 1);
                    break;
                }
            }
            // Remove tracks where client is the origin
            if (track->origin == removed) {
                arr_splice(&mediatracks, i, 1);
                --i;
            }
        }
    }
}

static void handle_intent(alloserver* serv, alloserver_client* client, allo_client_intent *intent)
{
  (void)serv;
  allo_client_intent_clone(intent, client->intent);
  free(client->intent->entity_id);
  client->intent->entity_id = allo_strdup(client->avatar_entity_id);
}

static void handle_place_announce_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  const int version = cJSON_GetArrayItem(body, 2)->valueint; (void)version;
  cJSON* identity = cJSON_GetArrayItem(body, 4);
  cJSON* avatar = cJSON_DetachItemFromArray(body, 6);

  allo_entity *ava = allo_state_add_entity_from_spec(&serv->state, client->agent_id, avatar, NULL);// takes avatar
  client->avatar_entity_id = allo_strdup(ava->id);
  client->identity = cJSON_Duplicate(identity, true);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("announce"), cJSON_CreateString(ava->id), cJSON_CreateString(g_placename), NULL);
  char* respbodys = cJSON_Print(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  free(respbodys);
  cJSON_Delete(respbody);
  send_interaction_to_client(serv, client, response);
  allo_interaction_free(response);
}

static void handle_place_spawn_entity_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON* edesc = cJSON_DetachItemFromArray(body, 1);

  allo_entity *entity = allo_state_add_entity_from_spec(&serv->state, client->agent_id, edesc, NULL); // takes edesc

  cJSON* respbody = cjson_create_list(cJSON_CreateString("spawn_entity"), cJSON_CreateString(entity->id), NULL);
  char* respbodys = cJSON_Print(respbody);
  cJSON_Delete(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  free(respbodys);
  send_interaction_to_client(serv, client, response);
  allo_interaction_free(response);
}

static void handle_place_remove_entity_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON *jeid = cJSON_DetachItemFromArray(body, 1);
  cJSON *jmodes = cJSON_DetachItemFromArray(body, 2);
  const char *eid = cJSON_GetStringValue(jeid);
  const char *modes = cJSON_GetStringValue(jmodes);
  allo_removal_mode mode = AlloRemovalCascade;
  if(modes && strcmp(modes, "reparent") == 0)
  {
    mode = AlloRemovalReparent;
  }
  bool ok = allo_state_remove_entity_id(&serv->state, eid, mode);
  cJSON_Delete(jeid);
  cJSON_Delete(jmodes);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("remove_entity"), cJSON_CreateString(ok?"ok":"failed"), NULL);
  char* respbodys = cJSON_Print(respbody);
  cJSON_Delete(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  free(respbodys);
  
  send_interaction_to_client(serv, client, response);
  allo_interaction_free(response);
}

static void handle_place_change_components_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON* entity_id = cJSON_GetArrayItem(body, 1);
  cJSON* comps = cJSON_DetachItemFromArray(body, 3);
  cJSON* rmcomps = cJSON_GetArrayItem(body, 5);
  cJSON* respbody = NULL;

  allo_entity* entity = state_get_entity(&serv->state, entity_id->valuestring);
  if(entity == NULL)
  {
    cJSON_Delete(comps);
    fprintf(stderr, "warning: trying to change comp on non-existing entity %s\n", entity_id->valuestring);
    respbody = cjson_create_list(cJSON_CreateString("change_components"), cJSON_CreateString("error"), cJSON_CreateString("no such entity"), NULL);
    goto end;
  }
  for (cJSON* comp = comps->child; comp != NULL;) 
  {
    cJSON* next = comp->next;
    cJSON_DeleteItemFromObject(entity->components, comp->string);
    cJSON_DetachItemViaPointer(comps, comp);
    cJSON_AddItemToObject(entity->components, comp->string, comp);
    comp = next;
  }

  assert(cJSON_GetArraySize(comps) == 0);
  cJSON_Delete(comps);

  cJSON* compname;
  cJSON_ArrayForEach(compname, rmcomps)
  {
    cJSON_DeleteItemFromObject(entity->components, compname->valuestring);
  }

  respbody = cjson_create_list(cJSON_CreateString("change_components"), cJSON_CreateString("ok"), NULL);
end:;
  char* respbodys = cJSON_Print(respbody);
  cJSON_Delete(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  free(respbodys);
  send_interaction_to_client(serv, client, response);
  allo_interaction_free(response);
}

static int next_free_track_id = 1;

static void handle_place_allocate_track_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    allo_entity* entity = state_get_entity(&serv->state, interaction->sender_entity_id);
    cJSON *existing_comp = entity ? cJSON_GetObjectItem(entity->components, "live_media") : NULL;

    if(!entity || existing_comp != NULL)
    {
      fprintf(stderr, "Disallowed creating allocating track for %s: entity already has track\n", interaction->sender_entity_id);
      cJSON* respbody = cjson_create_list(cJSON_CreateString("allocate_track"), cJSON_CreateString("failed"), cJSON_CreateString("only one track allowed per entity"), NULL);
      char* respbodys = cJSON_Print(respbody);
      cJSON_Delete(respbody);
      allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
      free(respbodys);
      send_interaction_to_client(serv, client, response);
      allo_interaction_free(response);
      return;
    }

    cJSON *media_type = cJSON_GetArrayItem(body, 1);
    cJSON *media_format = cJSON_GetArrayItem(body, 2);
    cJSON *media_metadata = cJSON_GetArrayItem(body, 3);

    int track_id = next_free_track_id++;
    cJSON *mediacomp = cjson_create_object(
        "track_id", cJSON_CreateNumber(track_id),
        "type", cJSON_Duplicate(media_type, false),
        "format", cJSON_Duplicate(media_format, false),
        "metadata", cJSON_Duplicate(media_metadata, true),
        NULL
    );

    fprintf(stderr, "Allocated track %d for %s.\n", track_id, interaction->sender_entity_id);

    
    allo_media_track *track = _media_track_find_or_create(&mediatracks, track_id, _media_track_type_from_string(media_type->valuestring));
    track->origin = client;

    cJSON_AddItemToObject(entity->components, "live_media", mediacomp);

    cJSON* respbody = cjson_create_list(cJSON_CreateString("allocate_track"), cJSON_CreateString("ok"), cJSON_CreateNumber(track_id), NULL);
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
}

static void handle_place_media_track_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body) {
    cJSON *jTrackId = cJSON_GetArrayItem(body, 1);
    cJSON *jsub = cJSON_GetArrayItem(body, 2);
    cJSON* respbody;

    if (!cJSON_IsNumber(jTrackId) || !cJSON_IsString(jsub)) {
      respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("failed"), cJSON_CreateString("missing id or verb"), NULL);
      fprintf(stderr, "malformed media_track interaction");
      goto done;
    }
    
    uint32_t track_id = jTrackId->valueint;
    
    // find the track and add or remove client to list of recipients
    allo_media_track *track = _media_track_find(&mediatracks, track_id);
    if(!track) {
      respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("failed"), cJSON_CreateString("invalid track id"), NULL);
      fprintf(stderr, "media_track: invalid track id");
      goto done;
    }
    if (strcmp(jsub->valuestring, "subscribe") == 0) {
        arr_push(&track->recipients, client);
    } else if (strcmp(jsub->valuestring, "unsubscribe") == 0) {
        for (size_t i = 0; i < track->recipients.length; i++) {
            if (track->recipients.data[i] == client) {
                arr_splice(&track->recipients, i, 1);
                break;
            }
        }
    } else {
      respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("failed"), cJSON_CreateString("incorrect verb"), NULL);
      fprintf(stderr, "media_track: neither sub nor unsub");
      goto done;
    }
    respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("ok"), cJSON_CreateNumber(track_id), NULL);
  
done:;
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
}

static void handle_place_add_property_animation_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    allo_entity* entity = state_get_entity(&serv->state, interaction->sender_entity_id);
    cJSON *animation_spec = cJSON_DetachItemFromArray(body, 1);
    bool ok = cJSON_HasObjectItem(animation_spec, "path") && cJSON_HasObjectItem(animation_spec, "from") && cJSON_HasObjectItem(animation_spec, "to");
    cJSON* respbody;

    if(ok)
    {
        cJSON *comp = cJSON_GetObjectItemCaseSensitive(entity->components, "property_animations");
        if(!comp)
        {
            comp = cjson_create_object("animations", cJSON_CreateObject(), NULL);
            cJSON_AddItemToObjectCS(entity->components, "property_animations", comp);
        }
        cJSON *anims = cJSON_GetObjectItemCaseSensitive(comp, "animations");
        char anim_id[9];
        allo_generate_id(anim_id, 9);
        cJSON_AddItemToObject(anims, anim_id, animation_spec);

        respbody = cjson_create_list(cJSON_CreateString("add_property_animation"), cJSON_CreateString("ok"), cJSON_CreateString(anim_id), NULL);
    } else {
        respbody = cjson_create_list(cJSON_CreateString("add_property_animation"), cJSON_CreateString("failed"), cJSON_CreateString("invalid argument"), NULL);
    }

    
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
}

static void handle_place_remove_property_animation_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    allo_entity* entity = state_get_entity(&serv->state, interaction->sender_entity_id);
    const char *animation_id = cJSON_GetStringValue(cJSON_GetArrayItem(body, 1));
    cJSON* respbody;
    bool ok = animation_id != NULL;
    const char *errstr = "missing animation ID";

    if(ok) {
      cJSON *comp = cJSON_GetObjectItemCaseSensitive(entity->components, "property_animations");
      cJSON *anims = cJSON_GetObjectItemCaseSensitive(comp, "animations");
      cJSON *anim = cJSON_DetachItemFromObjectCaseSensitive(anims, animation_id);
      ok = anim != NULL;
      errstr = "can't remove that animation because it doesn't exist";
      cJSON_Delete(anim);
    }

    if(ok)
    {
        respbody = cjson_create_list(cJSON_CreateString("remove_property_animation"), cJSON_CreateString("ok"), cJSON_CreateString(animation_id), NULL);
    } else {
        respbody = cjson_create_list(cJSON_CreateString("remove_property_animation"), cJSON_CreateString("failed"), cJSON_CreateString(errstr), NULL);
    }
    
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
}

static void handle_place_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction)
{
    cJSON* body = cJSON_Parse(interaction->body);
    const char *name = cJSON_GetArrayItem(body, 0)->valuestring;
    if (strcmp(name, "announce") == 0) {
        handle_place_announce_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "change_components") == 0) {
        handle_place_change_components_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "spawn_entity") == 0) {
        handle_place_spawn_entity_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "remove_entity") == 0) {
        handle_place_remove_entity_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "allocate_track") == 0) {
        handle_place_allocate_track_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "media_track") == 0) {
        handle_place_media_track_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "add_property_animation") == 0) {
        handle_place_add_property_animation_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "remove_property_animation") == 0) {
        handle_place_remove_property_animation_interaction(serv, client, interaction, body);
    }

  // force sending delta, since the above was likely an important change
  last_simulate_at = 0;

  cJSON_Delete(body);
}

static void handle_interaction(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  if (strcmp(interaction->receiver_entity_id, "place") == 0)
  {
    handle_place_interaction(serv, client, interaction);
  }
  else
  {
    allo_entity* entity = NULL;
    LIST_FOREACH(entity, &serv->state.entities, pointers) {
      if (strcmp(entity->id, interaction->receiver_entity_id) == 0) {
        alloserver_client* client2;
        LIST_FOREACH(client2, &serv->clients, pointers) {
          if (strcmp(entity->owner_agent_id, client2->agent_id) == 0) {
            send_interaction_to_client(serv, client2, interaction);
            return;
          }
        }
        break;
      }
    }
    // TODO: send failure response, because recipient was not found.
  }
}

static void handle_clock(alloserver *serv, alloserver_client *client, cJSON *cmd)
{
  cJSON *server_time = cJSON_GetObjectItemCaseSensitive(cmd, "server_time");
  if(server_time == NULL)
    server_time = cJSON_AddNumberToObject(cmd, "server_time", 0.0);
  cJSON_SetNumberValue(server_time, get_ts_monod());

  const char* json = cJSON_Print(cmd);
  serv->send(serv, client, CHANNEL_CLOCK, (const uint8_t*)json, strlen(json));
  free((void*)json);
}

static void handle_media(alloserver *serv, alloserver_client *client, const uint8_t *data, size_t length)
{
    // get the track_id from the top of data
    uint32_t track_id;
    assert(length >= sizeof(track_id) + 3);
    memcpy(&track_id, data, sizeof(track_id));
    track_id = ntohl(track_id);
    
    // check agains list of open tracks
    allo_media_track *track = _media_track_find(&mediatracks, track_id);
    
    // ignore this data if track was never allocated
    if (track == NULL) {
        return;
    }
    
    // igore if the sender is not client that allocated the track
    if (track->origin != client) {
        return;
    }
    
    // pass information on to all peers in track recipient list
    for (size_t i = 0; i < track->recipients.length; i++) {
        ENetPacket *packet = enet_packet_create(data, length, 0);
        alloserv_send_enet(serv, track->recipients.data[i], CHANNEL_MEDIA, packet);
    }
}

static void received_from_client(alloserver* serv, alloserver_client* client, allochannel channel, const uint8_t* data, size_t data_length)
{
    if (channel == CHANNEL_STATEDIFFS) {
        cJSON* cmd = cJSON_ParseWithLength((const char*)data, data_length);
        const cJSON* ntvintent = cJSON_GetObjectItemCaseSensitive(cmd, "intent");
        allo_client_intent *intent = allo_client_intent_parse_cjson(ntvintent);
        handle_intent(serv, client, intent);
        allo_client_intent_free(intent);
        cJSON_Delete(cmd);
    } else if (channel == CHANNEL_COMMANDS) {
        cJSON* cmd = cJSON_ParseWithLength((const char*)data, data_length);
        allo_interaction* interaction = allo_interaction_parse_cjson(cmd);
        handle_interaction(serv, client, interaction);
        allo_interaction_free(interaction);
        cJSON_Delete(cmd);
    } else if (channel == CHANNEL_CLOCK) {
        cJSON* cmd = cJSON_ParseWithLength((const char*)data, data_length);
        handle_clock(serv, client, cmd);
        cJSON_Delete(cmd);
    } else if (channel == CHANNEL_MEDIA) {
        handle_media(serv, client, data, data_length);
    }
}

static statehistory_t hist;
static void broadcast_server_state(alloserver* serv)
{
  serv->state.revision++;
  // roll over revision to 0 before it reaches biggest consecutive integer representable in json
  if(serv->state.revision == 9007199254740990) { serv->state.revision = 0; }

  cJSON *map = allo_state_to_json(&serv->state, false);
  allo_delta_insert(&hist, map);

  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    char *json = allo_delta_compute(&hist, client->intent->ack_state_rev);
    int jsonlength = strlen(json);
    serv->send(serv, client, CHANNEL_STATEDIFFS, (const uint8_t*)json, jsonlength);
    free(json);
  }
}

static void step(double goalDt)
{
  while (serv->interbeat(serv, 1)) {}

  double now = get_ts_monod();

  if (last_simulate_at + goalDt > now) {
    return;
  }
  last_simulate_at = now;

  allo_client_intent *intents[32];
  int count = 0;
  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    intents[count++] = client->intent;
    if (count == 32) break;
  }
  allo_simulate(&serv->state, (const allo_client_intent**)intents, count, now);
  broadcast_server_state(serv);
}

cJSON* cjson3d(double a, double b, double c)
{
  return cjson_create_list( cJSON_CreateNumber(a), cJSON_CreateNumber(b), cJSON_CreateNumber(c), NULL);
}

cJSON* cjson2d(double a, double b)
{
  return cjson_create_list(cJSON_CreateNumber(a), cJSON_CreateNumber(b), NULL);
}

static cJSON* spec_located_at(float x, float y, float z, float sz)
{
  return cjson_create_object(
    "transform", cjson_create_object(
      "matrix", m2cjson(allo_m4x4_translate((allo_vector) {{ x, y, z }})),
      NULL
    ),
    "geometry", cjson_create_object(
      "type", cJSON_CreateString("inline"),
      "vertices", cjson_create_list(cjson3d(sz, 0.0, -sz), cjson3d(sz, 0.0, sz), cjson3d(-sz, sz, -sz), cjson3d(-sz, sz, sz), NULL),
      "uvs", cjson_create_list(cjson2d(0.0, 0.0), cjson2d(1.0, 0.0), cjson2d(0.0, 1.0), cjson2d(1.0, 1.0), NULL),
      "triangles", cjson_create_list(cjson3d(0, 3, 1), cjson3d(0, 2, 3), cjson3d(1, 3, 0), cjson3d(3, 2, 0), NULL),
      "texture", cJSON_CreateString("iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAD8SURBVGhD7c/LCcJgFERhq7Qgy3CfRXoQXItNGYmEeMyQbEbuhYFv9T9gzun6fPR1HofGAdP6xgHz+q4By/qWAev1/QKwftIpANNnbQKwe9EjAKPXGgRgMVQPwNxfpQOwddPRgMv99mcYqiTABkOVBNhgqJIAGwxVEmCDoUoCbDBUSYANhioJsMFQJQE2GKokwAZDlQTYYKiSABsMVRJgg6FKAmwwVEmADYYqCbDBUCUBNhiqJMAGQ5UE2GCokgAbDFUSYIOhytEAfKvjUAD+lLIfgA/V7ATgdUEyAO/K2g7Ao8o2AvCiOAbgur6vANy18AnAaSPvABx1Mg4vbr0dVP2tGoQAAAAASUVORK5CYII="),
      NULL
    ),
    NULL
  );
}
static cJSON* spec_add_child(cJSON* spec, cJSON* childspec)
{
  cJSON* children = cJSON_GetObjectItemCaseSensitive(spec, "children");
  if (children == NULL) {
    children = cJSON_CreateArray();
    cJSON_AddItemToObject(spec, "children", children);
  }
  cJSON_AddItemToArray(children, childspec);
  return spec;
}


void add_dummy(alloserver *serv)
{
  cJSON* root = spec_located_at(0, 0, 0, 0.3);
  cJSON *plate = spec_located_at(0, 0.9, 0, 0.2);
  spec_add_child(root, plate);
  cJSON* button = spec_located_at(0.2, 0.3, 0, 0.1);
  spec_add_child(plate, button);

  cJSON_AddItemToObject(button, "collider", cjson_create_object(
    "type", cJSON_CreateString("box"),
    "width", cJSON_CreateNumber(0.2),
    "height", cJSON_CreateNumber(0.2),
    "depth", cJSON_CreateNumber(0.2),
    NULL
  ));
  cJSON_AddItemToObject(button, "grabbable", cjson_create_object(
    "actuate_on", cJSON_CreateString("$parent"),
    "translation_constraint", cjson_create_list(cJSON_CreateNumber(1), cJSON_CreateNumber(0), cJSON_CreateNumber(1)),
    "rotation_constraint", cjson_create_list(cJSON_CreateNumber(0), cJSON_CreateNumber(1), cJSON_CreateNumber(0)),
    NULL
  ));

  cJSON_AddItemToObject(plate, "grabbable", cjson_create_object(
    "foo", cJSON_CreateString("bar"),
    NULL
  ));
  cJSON_AddItemToObject(plate, "collider", cjson_create_object(
    "type", cJSON_CreateString("box"),
    "width", cJSON_CreateNumber(0.2),
    "height", cJSON_CreateNumber(0.2),
    "depth", cJSON_CreateNumber(0.2),
    NULL
  ));

  cJSON_AddItemToObject(root, "grabbable", cjson_create_object(
    "foo", cJSON_CreateString("bar"),
    NULL
  ));
  cJSON_AddItemToObject(root, "collider", cjson_create_object(
    "type", cJSON_CreateString("box"),
    "width", cJSON_CreateNumber(0.2),
    "height", cJSON_CreateNumber(0.2),
    "depth", cJSON_CreateNumber(0.2),
    NULL
  ));

  allo_state_add_entity_from_spec(&serv->state, NULL, root, NULL);
}

static allo_entity* add_place(alloserver *serv)
{
  cJSON* place = cjson_create_object(
    "transform", cjson_create_object(
      "matrix", m2cjson(allo_m4x4_translate((allo_vector) {{ 0, 0, 0 }})),
      NULL
    ),
    "clock", cjson_create_object(
      "time", cJSON_CreateNumber(0.0),
      NULL
    ),
    NULL
  );

  allo_entity *e = allo_state_add_entity_from_spec(&serv->state, NULL, place, NULL);
  free(e->id); e->id = strdup("place");
  return e;
}

bool alloserv_run_standalone(int host, int port, const char *placename)
{
    alloserver *serv = alloserv_start_standalone(host, port, placename);
    arr_init(&mediatracks);
  
    if (serv == NULL)
    {
        return false;
    }
    int allosocket = allo_socket_for_select(serv);

    while (1) {
        if (alloserv_poll_standalone(allosocket) == false)
        {
            alloserv_stop_standalone();
            return false;
        }
    }

    alloserv_stop_standalone();

    return true;
}

alloserver *alloserv_start_standalone(int listenhost, int port, const char *placename)
{
  if (!allo_initialize(false))
  {
    fprintf(stderr, "Unable to initialize allostate");
    return NULL;
  }

  assert(serv == NULL);

  g_placename = strdup(placename);

  int retries = 3;
  while (!serv)
  {
    serv = allo_listen(listenhost, port);
    if (!serv) {
      fprintf(stderr, "Unable to open listen socket, ");
      if (retries-- > 0) {
        fprintf(stderr, "retrying %d more times...\n", retries);
        // todo: sleep for 1s
      }
      else {
        fprintf(stderr, "giving up. Is another server running?\n");
        break;
      }
    }
  }

  if (!serv) {
    perror("errno");
    alloserv_stop_standalone();
    return NULL;
  }
  serv->clients_callback = clients_changed;
  serv->raw_indata_callback = received_from_client;
  allo_state_init(&serv->state);

  fprintf(stderr, "alloserv_run_standalone open on port %d\n", serv->_port);
  place = add_place(serv);

  return serv;
}

bool alloserv_poll_standalone(int allosocket)
{
  ENetSocketSet set;
  ENET_SOCKETSET_EMPTY(set);
  ENET_SOCKETSET_ADD(set, allosocket);

  int hz = 5;
  double dt = 1.0/hz;
  int dtmillis = dt*1000;

  int selectr = enet_socketset_select(allosocket, &set, NULL, dtmillis);
  if (selectr < 0 && errno != EINTR) {
    perror("select failed, terminating");
    return false;
  }
  else
  {
    step(dt);
  }
  return true;
}

void alloserv_stop_standalone()
{
  if(serv) alloserv_stop(serv);
  place = NULL;
  serv = NULL;
}
