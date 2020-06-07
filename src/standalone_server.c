#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>

#include <allonet/allonet.h>
#include "util.h"

static alloserver* serv;

static void send_interaction_to_client(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  cJSON* cmdrep = allo_interaction_to_cjson(interaction);
  const char* json = cJSON_Print(cmdrep);
  cJSON_Delete(cmdrep);

  serv->send(serv, client, CHANNEL_COMMANDS, json, strlen(json)+1);
  free((void*)json);
}

static void clients_changed(alloserver* serv, alloserver_client* added, alloserver_client* removed)
{
  // todo: delete entities for disconnected client
}

static void handle_intent(alloserver* serv, alloserver_client* client, allo_client_intent *intent)
{
  allo_client_intent_clone(intent, client->intent);
  free(client->intent->entity_id);
  client->intent->entity_id = strdup(client->avatar_entity_id);
}

static void handle_place_announce_interaction(alloserver* serv, alloserver_client* client, allo_interaction* interaction, cJSON *body)
{
  const int version = cJSON_GetArrayItem(body, 2)->valueint;
  cJSON* identity = cJSON_GetArrayItem(body, 4);
  cJSON* avatar = cJSON_DetachItemFromArray(body, 6);

  allo_entity *ava = allo_state_add_entity_from_spec(&serv->state, client->agent_id, avatar, NULL);
  client->avatar_entity_id = strdup(ava->id);

  cJSON* respbody = cjson_create_list(cJSON_CreateString("announce"), cJSON_CreateString(ava->id), cJSON_CreateString("Menu"), NULL);
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
  cJSON* comp = NULL;
  for (cJSON* comp = comps->child; comp != NULL;)
  {
    cJSON* next = comp->next;
    cJSON_DeleteItemFromObject(entity->components, comp->string);
    cJSON_DetachItemViaPointer(comps, comp);
    cJSON_AddItemToObject(entity->components, comp->string, comp);
    comp = next;
  }

  cJSON_ArrayForEach(comp, rmcomps)
  {
    cJSON_DeleteItemFromObject(entity->components, comp->valuestring);
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

static void received_from_client(alloserver* serv, alloserver_client* client, allochannel channel, const uint8_t* data, size_t data_length)
{
  cJSON* cmd = cJSON_Parse(data);
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
  cJSON_Delete(cmd);
}

static void broadcast_server_state(alloserver* serv)
{
  cJSON* entities_rep = cJSON_CreateArray();
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &serv->state.entities, pointers) {
    cJSON* entity_rep = cjson_create_object(
      "id", cJSON_CreateString(entity->id),
      NULL
    );
    cJSON_AddItemReferenceToObject(entity_rep, "components", entity->components);
    cJSON_AddItemToArray(entities_rep, entity_rep);
  }
  cJSON* map = cjson_create_object(
    "entities", entities_rep,
    "revision", cJSON_CreateNumber(serv->state.revision++),
    NULL
  );
  char* json = cJSON_Print(map);
  cJSON_Delete(map);

  int jsonlength = strlen(json);

  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    serv->send(serv, client, CHANNEL_STATEDIFFS, json, jsonlength + 1);
  }
  free(json);
}

static void step(double dt)
{
  while (serv->interbeat(serv, 1)) {}

  allo_client_intent *intents[32];
  int count = 0;
  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    intents[count++] = client->intent;
    if (count == 32) break;
  }
  allo_simulate(&serv->state, dt, intents, count);
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
      "matrix", m2cjson(allo_m4x4_translate((allo_vector) { x, y, z })),
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
    NULL
  ));

  allo_state_add_entity_from_spec(&serv->state, NULL, root, NULL);
}

bool alloserv_run_standalone(int port)
{
  if (!allo_initialize(false))
  {
    fprintf(stderr, "Unable to initialize allostate");
    return false;
  }

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
    return false;
  }
  serv->clients_callback = clients_changed;
  serv->raw_indata_callback = received_from_client;
  LIST_INIT(&serv->state.entities);

  fprintf(stderr, "alloserv_run_standalone open on port %d\n", port);
  int allosocket = allo_socket_for_select(serv);

  add_dummy(serv);

  while (1) {
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
  }

  return true;
}