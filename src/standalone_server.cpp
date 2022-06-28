#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>
#include <errno.h>
#include "httplib.h"

#include <string>
#include <vector>

#include <allonet/allonet.h>
#include "media/media.h"
#include "util.h"
#include "delta.h"
#include "uri.h"

static alloserver* serv;
static allo_entity* place;
static double last_simulate_at = 0;
static char *g_placename;
static allo_media_track_list mediatracks;

typedef struct {
    std::string avatarToken;
    allo_interaction *inter;
} OutstandingAppLaunchRequest;
static std::vector<OutstandingAppLaunchRequest> outstanding_app_launch_requests;
static void handle_app_launched(alloserver* serv, std::string avatarToken, allo_entity *ava);

// local helpers

static void send_interaction_to_client(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  cJSON* cmdrep = allo_interaction_to_cjson(interaction);
  const char* json = cJSON_Print(cmdrep);
  cJSON_Delete(cmdrep);

  serv->send(serv, client, CHANNEL_COMMANDS, (const uint8_t*)json, strlen(json));
  free((void*)json);
}

static allo_entity *find_service(allo_state *state, const char *by_servicename)
{
    if(!by_servicename) return NULL;
    
    allo_entity* entity = NULL;
    LIST_FOREACH(entity, &state->entities, pointers)
    {
        cJSON *comps = entity->components;
        cJSON *servdisc = cJSON_GetObjectItemCaseSensitive(comps, "service_discovery");
        cJSON *servnamej = cJSON_GetObjectItemCaseSensitive(servdisc, "name");
        const char *servname = cJSON_GetStringValue(servnamej);

        if (servname && strcmp(servname, by_servicename) == 0)
        {
            return entity;
        }
    }

    return NULL;
}

static alloserver_client *find_agent_by_id(alloserver *serv, const char *by_id)
{
    if(!by_id) return NULL;

    alloserver_client *agent;
    LIST_FOREACH(agent, &serv->clients, pointers) {
        if(strcmp(agent->agent_id, by_id) == 0)
        {
            return agent;
        }
    }
    return NULL;
}

// callbacks
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

// interactions

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
  cJSON* avatar = cJSON_DetachItemFromArray(body, 6);
  
  cJSON *avatarC = cJSON_DetachItemFromObjectCaseSensitive(avatar, "avatar");
  cJSON *avatarTokenj = cJSON_GetObjectItemCaseSensitive(avatarC, "token");
  std::string avatarToken = avatarTokenj ? std::string(cJSON_GetStringValue(avatarTokenj)) : "";
  cJSON_Delete(avatarC);

  allo_entity *ava = allo_state_add_entity_from_spec(&serv->state, client->agent_id, avatar, NULL);// takes avatar
  client->avatar_entity_id = allo_strdup(ava->id);
  client->identity = cJSON_Duplicate(identity, true);

  if(avatarTokenj) {
    handle_app_launched(serv, avatarToken, ava);
  }


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
    cJSON* respbody = cjson_create_list(cJSON_CreateString("announce"), cJSON_CreateString(ava->id), cJSON_CreateString(g_placename), NULL);
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

    fprintf(stderr, "Allocated track %d (%s.%s) for %s/%s.\n", 
      track_id, cJSON_GetStringValue(media_type), cJSON_GetStringValue(media_format),
      interaction->sender_entity_id, alloserv_describe_client(client)
    );

    
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
    char* respbodys;
    uint32_t track_id;
    allo_media_track *track;

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
    respbodys = cJSON_Print(respbody);
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

static void handle_place_list_agents_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
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
}

static void handle_place_kick_agent_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    const char *agent_id = cJSON_GetStringValue(cJSON_GetArrayItem(body, 1));
    if(!agent_id) 
    {
        handle_invalid_place_interaction(serv, client, interaction, body);
        return;
    }
    
    alloserver_client *agent = find_agent_by_id(serv, agent_id);
    cJSON *respbody;
    if(agent)
    {
        alloserv_disconnect(serv, agent, alloerror_kicked_by_admin);
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

// {"launch_app", "alloapp:http://host:port/{appid or path}", args}
static void handle_place_launch_app_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
    std::string app_url = cJSON_GetStringValue(cJSON_GetArrayItem(body, 1));
    cJSON *launch_args = cJSON_DetachItemFromArray(body, 2); // can be NULL/missing
    if(!launch_args)
        launch_args = cJSON_CreateObject();
    char avatar_token[20];
    allo_generate_id(avatar_token, 20);
    cJSON_AddItemToObjectCS(launch_args, "avatarToken", cJSON_CreateString(avatar_token));
    
    std::string prefix = "alloapp:";
    if(app_url.length() == 0 || app_url.find(prefix) != 0)
    {
        handle_invalid_place_interaction(serv, client, interaction, body);
        cJSON_Delete(launch_args);
        return;
    }

    allo_entity *reqent = state_get_entity(&serv->state, interaction->sender_entity_id);
    alloserver_client *requestor = find_agent_by_id(serv, reqent->owner_agent_id);
    char *identitys = cJSON_Print(requestor->identity); 
    std::string identity = identitys; 
    free(identitys);

    char *launch_argss = cJSON_Print(launch_args);
    httplib::Headers headers = {
      {"x-alloverse-server", "alloplace://localhost"}, // todo: use real hostname
      {"x-alloverse-identity", identity}, // todo: use real hostname
    };

    // ask the app to connect to us
    std::string httpurl = app_url.substr(prefix.length());
    Uri httpuri = Uri::Parse(httpurl);
    httplib::Client webclient(httpuri.HostWithPort());
    httplib::Result res = webclient.Post(httpuri.PathWithQuery().c_str(), headers, launch_argss, strlen(launch_argss), "application/json");
    free(launch_argss);
    // todo: handle launch results somehow
    // todo: don't block thread :S

    // save it so we can respond when we get a connection from the gateway.
    OutstandingAppLaunchRequest req = {avatar_token, allo_interaction_clone(interaction)};
    outstanding_app_launch_requests.push_back(req);
    cJSON_Delete(launch_args);
}

static void handle_app_launched(alloserver* serv, std::string avatarToken, allo_entity *ava)
{
    auto req_it = find_if(outstanding_app_launch_requests.begin(), outstanding_app_launch_requests.end(),
        [&avatarToken] (const OutstandingAppLaunchRequest& r) { 
            return r.avatarToken == avatarToken; 
        });
    if(req_it == outstanding_app_launch_requests.end())
    {
        fprintf(stderr, "Warning: app launched with avatar token %s, but no such request found.\n", avatarToken.c_str());
        return;
    }
    auto req = *req_it;
    outstanding_app_launch_requests.erase(req_it);

    allo_entity *requestor = state_get_entity(&serv->state, req.inter->sender_entity_id);
    if(!requestor) {
        fprintf(stderr, "Warning: app launched with avatar token %s, but no such requestor found.\n", avatarToken.c_str());
        return;
    }
    alloserver_client *client = find_agent_by_id(serv, requestor->owner_agent_id);
    if(!client) {
        fprintf(stderr, "Warning: app launched with avatar token %s, but no such requestor agent found.\n", avatarToken.c_str());
        return;
    }

    cJSON *respbody = cjson_create_list(cJSON_CreateString("launch_app"), cJSON_CreateString("ok"), cJSON_CreateString(ava->id), NULL);
    char* respbodys = cJSON_Print(respbody);
    cJSON_Delete(respbody);

    allo_interaction* response = allo_interaction_create("response", "place", req.inter->sender_entity_id, req.inter->request_id, respbodys);
    free(respbodys);
    send_interaction_to_client(serv, client, response);
    allo_interaction_free(response);
    allo_interaction_free(req.inter);
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
    } else if (strcmp(name, "launch_app") == 0) {
        handle_place_launch_app_interaction(serv, client, interaction, body);
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
        allo_entity* entity = state_get_entity(&serv->state, interaction->receiver_entity_id);
        if(entity)
        {
            alloserver_client* client2 = find_agent_by_id(serv, entity->owner_agent_id);
            if(client2)
            {
                send_interaction_to_client(serv, client2, interaction);
                return;
            }
        }
        cJSON *body = cJSON_Parse(interaction->body);
        handle_invalid_place_interaction(serv, client, interaction, body);
        cJSON_Delete(body);
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
    /// Note: The returned json is managed by allo_delta_compute
    char *json = allo_delta_compute(&hist, client->intent->ack_state_rev);
    int jsonlength = strlen(json);
    serv->send(serv, client, CHANNEL_STATEDIFFS, (const uint8_t*)json, jsonlength);
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
  allo_simulate(&serv->state, (const allo_client_intent**)intents, count, now, NULL);
  broadcast_server_state(serv);
}

static allo_entity* add_place(alloserver *serv)
{
  allo_vector origin{{0, 0, 0}};
  cJSON* place = cjson_create_object(
    "transform", cjson_create_object(
      "matrix", m2cjson(allo_m4x4_translate(origin)),
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

extern "C" bool alloserv_run_standalone(int host, int port, const char *placename)
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
