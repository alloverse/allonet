#define ALLO_INTERNALS 1

#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>
#include <errno.h>
#include <vector>
#include <string>
using namespace std;

#include <allonet/allonet.h>
#include <flatbuffers/idl.h>
#include "media/media.h"
#include "util.h"
#include "delta.h"
#include "alloverse_binary_schema.h"
using namespace Alloverse;

static alloserver* serv;
static allo_mutable_state state;
static EntityT *place;
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
    (void)serv;
    (void)added;
    if (removed) {
        state.removeEntitiesForAgent(removed->agent_id);
        
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

static void handle_invalid_place_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    if(strcmp(interaction->type, "request") != 0){
        return; // just ignore non-requests
    }

    cJSON *cmd = cJSON_GetArrayItem(body, 0);
    cJSON *respbody = cjson_create_list(cJSON_CreateString(cJSON_GetStringValue(cmd)), cJSON_CreateString("error"), cJSON_CreateString("invalid request"), NULL);
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
}

static void handle_place_announce_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  const int version = cJSON_GetArrayItem(body, 2)->valueint;
  cJSON* identity = cJSON_GetArrayItem(body, 4);
  cJSON* avatar = cJSON_GetArrayItem(body, 6);
  char *avatars = cJSON_Print(avatar);

  Alloverse::EntityT *ava = state.addEntityFromSpec(avatars, client->agent_id, NULL);
  free(avatars);
  client->avatar_entity_id = allo_strdup(ava->id.c_str());
  client->identity = cJSON_Duplicate(identity, true);


  if(version != GetAllonetProtocolVersion())
  {
    fprintf(stderr, "Client announced %s incompatible version %d, DISCONNECTING\n", alloserv_describe_client(client), version);
    cJSON* respbody = cjson_create_list(
      cJSON_CreateString("announce"), cJSON_CreateString("error"), 
      cJSON_CreateNumber(alloerror_outdated_version), cJSON_CreateString("Please update your app. Outdated network protocol."), NULL
    );
    char* respbodys = cJSON_Print(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    cJSON_Delete(respbody);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
    alloserv_disconnect(serv, client, alloerror_outdated_version);
  }
  else
  {
    fprintf(stderr, "Client announced: %s version %d\n", alloserv_describe_client(client), version);
    cJSON* respbody = cjson_create_list(cJSON_CreateString("announce"), cJSON_CreateString(ava->id.c_str()), cJSON_CreateString(g_placename), NULL);
    char* respbodys = cJSON_Print(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    cJSON_Delete(respbody);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
  }
}


static void handle_place_spawn_entity_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON* edesc = cJSON_GetArrayItem(body, 1);
  char *edescs = cJSON_Print(edesc);

  Alloverse::EntityT *entity = state.addEntityFromSpec(edescs, client->agent_id, NULL);
  free(edescs);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("spawn_entity"), cJSON_CreateString(entity->id.c_str()), NULL);
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
  bool ok = state.removeEntity(mode, eid);
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
  cJSON* comps = cJSON_GetArrayItem(body, 3);
  char *compss = cJSON_Print(comps);
  cJSON* rmcomps = cJSON_GetArrayItem(body, 5);
  cJSON* respbody = NULL;
  vector<string> componentKeysToRemove;
  cJSON* compname;
  cJSON_ArrayForEach(compname, rmcomps)
  {
    componentKeysToRemove.push_back(cJSON_GetStringValue(compname));
  }

  Alloverse::EntityT* entity = state.getEntity(entity_id->valuestring);
  if(entity == NULL)
  {
    cJSON_Delete(comps);
    fprintf(stderr, "warning: trying to change comp on non-existing entity %s\n", entity_id->valuestring);
    respbody = cjson_create_list(cJSON_CreateString("change_components"), cJSON_CreateString("error"), cJSON_CreateString("no such entity"), NULL);
    goto end;
  }
  
  state.changeComponents(entity, compss, componentKeysToRemove);

  respbody = cjson_create_list(cJSON_CreateString("change_components"), cJSON_CreateString("ok"), NULL);
end:;
  free(compss);
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
    const char *media_type = cJSON_GetStringValue(cJSON_GetArrayItem(body, 1));
    const char *media_format = cJSON_GetStringValue(cJSON_GetArrayItem(body, 2));
    cJSON *media_metadata = cJSON_GetArrayItem(body, 3);
    char *media_metadatas = cJSON_Print(media_metadata);
    cJSON* respbody;
    allo_media_track *track;
    int track_id;
    std::unique_ptr<LiveMediaComponentT> media;
    flatbuffers::Parser parser;

    parser.Parse((const char*)alloverse_schema_bytes);
    parser.SetRootType("LiveMediaMetadata");
    parser.Parse(media_metadatas);
    auto metadata = unique_ptr<LiveMediaMetadataT>(new LiveMediaMetadataT());
    flatbuffers::GetRoot<LiveMediaMetadata>(parser.builder_.GetBufferPointer())->UnPackTo(metadata.get());
    
    Alloverse::EntityT *entity = state.getNextEntity(interaction->sender_entity_id);
    if(!entity || !media_type || !media_format || !cJSON_IsObject(media_metadata))
    {
      fprintf(stderr, "Disallowed creating allocating track for %s: invalid argument\n", interaction->sender_entity_id);
      respbody = cjson_create_list(cJSON_CreateString("allocate_track"), cJSON_CreateString("failed"), cJSON_CreateString("invalid argument"), NULL);
      goto end;
    }
    
    if(entity->components->live_media.get())
    {
      fprintf(stderr, "Disallowed creating allocating track for %s: entity already has track\n", interaction->sender_entity_id);
      respbody = cjson_create_list(cJSON_CreateString("allocate_track"), cJSON_CreateString("failed"), cJSON_CreateString("only one track allowed per entity"), NULL);
      goto end;
    }

    track_id = next_free_track_id++;
    media = std::unique_ptr<LiveMediaComponentT>(new LiveMediaComponentT());
    media->track_id = track_id;
    media->type = media_type;
    media->format = media_format;
    media->metadata.swap(metadata);
    entity->components->live_media.swap(media);

    track = _media_track_find_or_create(&mediatracks, track_id, _media_track_type_from_string(media_type));
    track->origin = client;
    
    fprintf(stderr, "Allocated track %d (%s.%s) for %s/%s.\n", 
      track_id, media_type, media_format,
      interaction->sender_entity_id, alloserv_describe_client(client)
    );

    
    respbody = cjson_create_list(cJSON_CreateString("allocate_track"), cJSON_CreateString("ok"), cJSON_CreateNumber(track_id), NULL);

end:;
    free(media_metadatas);
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
    allo_media_track *track;
    uint32_t track_id;

    if (!cJSON_IsNumber(jTrackId) || !cJSON_IsString(jsub)) {
      respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("failed"), cJSON_CreateString("missing id or verb"), NULL);
      fprintf(stderr, "malformed media_track interaction");
      goto done;
    }
    
    track_id = jTrackId->valueint;
    
    // find the track and add or remove client to list of recipients
    track = _media_track_find(&mediatracks, track_id);
    if(!track) {
      respbody = cjson_create_list(cJSON_CreateString("media_track"), cJSON_CreateString("failed"), cJSON_CreateString("invalid track id"), NULL);
      fprintf(stderr, "media_track interaction: %s/%s requested unavailable track id %d\n", interaction->sender_entity_id, alloserv_describe_client(client), track_id);
      goto done;
    }
    if (strcmp(jsub->valuestring, "subscribe") == 0) {
        fprintf(stderr, "media_track interaction: %s/%s subscribed to track %d\n", interaction->sender_entity_id, alloserv_describe_client(client), track_id);
        arr_push(&track->recipients, client);
    } else if (strcmp(jsub->valuestring, "unsubscribe") == 0) {
        fprintf(stderr, "media_track interaction: %s/%s UNsubscribed to track %d\n", interaction->sender_entity_id, alloserv_describe_client(client), track_id);
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
  (void)serv; (void)client; (void)interaction; (void)body;
    /*
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
    */
}

static void handle_place_remove_property_animation_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    (void)serv; (void)client; (void)interaction; (void)body;
    /*
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
    */
}

static void handle_place_list_agents_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    (void)serv; (void)client; (void)interaction; (void)body;
    /*
    (void)body;
    cJSON *agentlist = cJSON_CreateArray();
    
    alloserver_client *agent;
    LIST_FOREACH(agent, &serv->clients, pointers) {
        allo_entity *ent = state_get_entity(&serv->state, agent->avatar_entity_id);
        cJSON *visor = cJSON_GetObjectItemCaseSensitive(ent->components, "visor");
        bool isVisor = visor != NULL;

        char stats[255];
        alloserv_get_client_stats(serv, agent, stats, 255, false);
        cJSON_AddItemToArray(agentlist, cjson_create_object(
            "display_name", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(agent->identity, "display_name"), false),
            "agent_id", cJSON_CreateString(agent->agent_id),
            "is_visor", cJSON_CreateBool(isVisor),
            "stats", cJSON_CreateString(stats),
            NULL, NULL
        ));
    }
    
    cJSON *respbody = cjson_create_list(cJSON_CreateString("list_agents"), cJSON_CreateString("ok"), agentlist, NULL);
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);
    allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
    */
}

static void handle_place_kick_agent_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    const char *agent_id = cJSON_GetStringValue(cJSON_GetArrayItem(body, 1));
    if(!agent_id) 
    {
        handle_invalid_place_interaction(serv, client, interaction, body);
        return;
    }
    
    bool found = false;
    alloserver_client *agent;
    LIST_FOREACH(agent, &serv->clients, pointers) {
        if(strcmp(agent->agent_id, agent_id) == 0)
        {
            alloserv_disconnect(serv, agent, alloerror_kicked_by_admin);
            found = true;
            break;
        }
    }
    
    cJSON *respbody;
    if(found)
    {
        respbody = cjson_create_list(cJSON_CreateString("list_agents"), cJSON_CreateString("ok"), NULL);
    }
    else
    {
        respbody = cjson_create_list(cJSON_CreateString("list_agents"), cJSON_CreateString("error"), cJSON_CreateString("agent not found"), NULL);
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
    } else if (strcmp(name, "list_agents") == 0) {
        handle_place_list_agents_interaction(serv, client, interaction, body);
    } else if (strcmp(name, "kick_agent") == 0) {
        handle_place_kick_agent_interaction(serv, client, interaction, body);
    } else {
        handle_invalid_place_interaction(serv, client, interaction, body);
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
    allo_entity* entity = state_get_entity(&state, interaction->receiver_entity_id);
    if(entity)
    {
      alloserver_client* client2;
      LIST_FOREACH(client2, &serv->clients, pointers) {
        if (strcmp(Alloverse_Entity_owner_agent_id(entity), client2->agent_id) == 0) {
          send_interaction_to_client(serv, client2, interaction);
          return;
        }
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

static void handle_media(alloserver *serv, alloserver_client *client, const uint8_t *data, size_t length, int channel)
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
    
    // ignore if the sender is not client that allocated the track
    if (track->origin != client) {
        return;
    }
    
    // pass information on to all peers in track recipient list
    for (size_t i = 0; i < track->recipients.length; i++) {
        ENetPacket *packet = enet_packet_create(data, length, 0);
        alloserv_send_enet(serv, (alloserver_client*)track->recipients.data[i], (allochannel)channel, packet);
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
    } else if (channel == CHANNEL_VIDEO || channel == CHANNEL_AUDIO) {
        handle_media(serv, client, data, data_length, channel);
    }
}


//static statehistory_t hist;
static void broadcast_server_state(alloserver* serv)
{
  state.finishIterationAndFlatten();

  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    serv->send(serv, client, CHANNEL_STATEDIFFS, (const uint8_t*)state.flat, state.flatlength);
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
  allo_simulate(&state, (const allo_client_intent**)intents, count, now, NULL);
  broadcast_server_state(serv);
}



static void addDefaultEntities(allo_mutable_state *mstate)
{
  
  place->id = "place";

  auto origin = unique_ptr<Mat4>(new Mat4(std::array<float, 16>{{1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1}}));
  auto transform = unique_ptr<TransformComponentT>(new TransformComponentT);
  transform->matrix = move(origin);
  place->components->transform = move(transform);

  auto clock = unique_ptr<ClockComponentT>(new ClockComponentT);
  clock->time = 0.0;
  place->components->clock = move(clock);
  
  mstate->next.entities.push_back(unique_ptr<EntityT>(place));
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
  serv->state = &state;

  fprintf(stderr, "alloserv_run_standalone open on port %d\n", serv->_port);
  addDefaultEntities(&state);

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
