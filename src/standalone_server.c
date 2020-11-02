#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>

#include <allonet/allonet.h>
#include "util.h"
#include "delta.h"

static alloserver* serv;
static allo_entity* place;

static void send_interaction_to_client(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  cJSON* cmdrep = allo_interaction_to_cjson(interaction);
  const char* json = cJSON_Print(cmdrep);
  cJSON_Delete(cmdrep);

  serv->send(serv, client, CHANNEL_COMMANDS, (const uint8_t*)json, strlen(json)+1);
  free((void*)json);
}

static void clients_changed(alloserver* serv, alloserver_client* added, alloserver_client* removed)
{
  if (removed) {
    allo_entity* entity = serv->state.entities.lh_first;
    while (entity)
    {
      allo_entity* to_delete = entity;
      entity = entity->pointers.le_next;
      if (strcmp(to_delete->owner_agent_id, removed->agent_id) == 0)
      {
        LIST_REMOVE(to_delete, pointers);
        entity_destroy(to_delete);
      }
    }
  }
}

static void handle_intent(alloserver* serv, alloserver_client* client, allo_client_intent *intent)
{
  allo_client_intent_clone(intent, client->intent);
  free(client->intent->entity_id);
  client->intent->entity_id = allo_strdup(client->avatar_entity_id);
}

static void handle_place_announce_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  const int version = cJSON_GetArrayItem(body, 2)->valueint; (void)version;
  cJSON* identity = cJSON_GetArrayItem(body, 4); (void)identity;
  cJSON* avatar = cJSON_DetachItemFromArray(body, 6);

  allo_entity *ava = allo_state_add_entity_from_spec(&serv->state, client->agent_id, avatar, NULL);
  client->avatar_entity_id = allo_strdup(ava->id);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("announce"), cJSON_CreateString(ava->id), cJSON_CreateString("Menu"), NULL);
  char* respbodys = cJSON_Print(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  cJSON_Delete(respbody);
  send_interaction_to_client(serv, client, response);
  free(respbodys);
}

static void handle_place_spawn_entity_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON* edesc = cJSON_DetachItemFromArray(body, 1);

  allo_entity *entity = allo_state_add_entity_from_spec(&serv->state, client->agent_id, edesc, NULL);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("spawn_entity"), cJSON_CreateString(entity->id), NULL);
  char* respbodys = cJSON_Print(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  cJSON_Delete(respbody);
  send_interaction_to_client(serv, client, response);
  free(respbodys);
}

static void handle_place_remove_entity_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  const char *eid = cJSON_GetStringValue(cJSON_DetachItemFromArray(body, 1));
  const char *modes = cJSON_GetStringValue(cJSON_DetachItemFromArray(body, 2));
  allo_removal_mode mode = AlloRemovalCascade;
  if(modes && strcmp(modes, "reparent") == 0)
  {
    mode == AlloRemovalReparent;
  }
  bool ok = allo_state_remove_entity(&serv->state, eid, mode);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("remove_entity"), cJSON_CreateString(ok?"ok":"failed"), NULL);
  char* respbodys = cJSON_Print(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  cJSON_Delete(respbody);
  send_interaction_to_client(serv, client, response);
  free(respbodys);
}

static void handle_place_change_components_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  cJSON* entity_id = cJSON_GetArrayItem(body, 1);
  cJSON* comps = cJSON_DetachItemFromArray(body, 3);
  cJSON* rmcomps = cJSON_GetArrayItem(body, 5);

  allo_entity* entity = state_get_entity(&serv->state, entity_id->valuestring);
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

  cJSON* respbody = cjson_create_list(cJSON_CreateString("change_components"), cJSON_CreateString("ok"), NULL);
  char* respbodys = cJSON_Print(respbody);
  allo_interaction* response = allo_interaction_create("response", "place", "", interaction->request_id, respbodys);
  cJSON_Delete(respbody);
  send_interaction_to_client(serv, client, response);
  free(respbodys);
}

static void handle_place_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction)
{
  cJSON* body = cJSON_Parse(interaction->body);
  if (strcmp(cJSON_GetArrayItem(body, 0)->valuestring, "announce") == 0)
  {
    handle_place_announce_interaction(serv, client, interaction, body);
  }
  else if (strcmp(cJSON_GetArrayItem(body, 0)->valuestring, "change_components") == 0)
  {
    handle_place_change_components_interaction(serv, client, interaction, body);
  }
  else if (strcmp(cJSON_GetArrayItem(body, 0)->valuestring, "spawn_entity") == 0)
  {
    handle_place_spawn_entity_interaction(serv, client, interaction, body);
  }
  else if (strcmp(cJSON_GetArrayItem(body, 0)->valuestring, "remove_entity") == 0)
  {
    handle_place_remove_entity_interaction(serv, client, interaction, body);
  }
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
  cJSON *server_time = cJSON_GetObjectItem(cmd, "server_time");
  if(server_time == NULL)
    server_time = cJSON_AddNumberToObject(cmd, "server_time", 0.0);
  cJSON_SetNumberValue(server_time, get_ts_monod());

  const char* json = cJSON_Print(cmd);
  serv->send(serv, client, CHANNEL_CLOCK, (const uint8_t*)json, strlen(json)+1);
  free((void*)json);
}

static void received_from_client(alloserver* serv, alloserver_client* client, allochannel channel, const uint8_t* data, size_t data_length)
{
  cJSON* cmd = cJSON_Parse((const char*)data);
  if (channel == CHANNEL_STATEDIFFS)
  {
    const cJSON* ntvintent = cJSON_GetObjectItem(cmd, "intent");
    allo_client_intent *intent = allo_client_intent_parse_cjson(ntvintent);
    handle_intent(serv, client, intent);
    allo_client_intent_free(intent);
  }
  else if (channel == CHANNEL_COMMANDS)
  {
    allo_interaction* interaction = allo_interaction_parse_cjson(cmd);
    handle_interaction(serv, client, interaction);
    allo_interaction_free(interaction);
  }
  else if (channel == CHANNEL_CLOCK)
  {
    handle_clock(serv, client, cmd);
  }
  cJSON_Delete(cmd);
}

static statehistory_t hist;
static void broadcast_server_state(alloserver* serv)
{
  serv->state.revision++;
  // roll over revision to 0 before it reaches biggest consecutive integer representable in json
  if(serv->state.revision == 9007199254740990) { serv->state.revision = 0; }

  cJSON *map = allo_state_to_json(&serv->state);
  allo_delta_insert(&hist, map);

  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    char *json = allo_delta_compute(&hist, client->intent->ack_state_rev);
    int jsonlength = strlen(json);
    serv->send(serv, client, CHANNEL_STATEDIFFS, (const uint8_t*)json, jsonlength + 1);
    free(json);
  }
}

static void step(double dt)
{
  while (serv->interbeat(serv, 1)) {}

  cJSON *clock = cJSON_GetObjectItem(place->components, "clock");
  cJSON_SetNumberValue(cJSON_GetObjectItem(clock, "time"), get_ts_monod());

  allo_client_intent *intents[32];
  int count = 0;
  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    intents[count++] = client->intent;
    if (count == 32) break;
  }
  allo_simulate(&serv->state, dt, (allo_client_intent**)intents, count);
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
  cJSON* children = cJSON_GetObjectItem(spec, "children");
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

bool alloserv_poll_standalone(int allosocket);
int alloserv_start_standalone(int port);

bool alloserv_run_standalone(int port)
{
  int allosocket = alloserv_start_standalone(port);
  if (allosocket == -1)
  {
    return false;
  }

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

int alloserv_start_standalone(int port)
{
  if (!allo_initialize(false))
  {
    fprintf(stderr, "Unable to initialize allostate");
    return -1;
  }

  assert(serv == NULL);

  int retries = 3;
  while (!serv)
  {
    serv = allo_listen(port);
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
    return -1;
  }
  serv->clients_callback = clients_changed;
  serv->raw_indata_callback = received_from_client;
  allo_state_init(&serv->state);

  fprintf(stderr, "alloserv_run_standalone open on port %d\n", port);
  int allosocket = allo_socket_for_select(serv);

  place = add_place(serv);

  return allosocket;
}

bool alloserv_poll_standalone(int allosocket)
{
  ENetSocketSet set;
  ENET_SOCKETSET_EMPTY(set);
  ENET_SOCKETSET_ADD(set, allosocket);

  int selectr = enet_socketset_select(allosocket, &set, NULL, 10);
  if (selectr < 0) {
    perror("select failed, terminating");
    return false;
  }
  else
  {
    step(0.01);
  }
  return true;
}

void alloserv_stop_standalone()
{
  alloserv_stop(serv);
  place = NULL;
  serv = NULL;
}
