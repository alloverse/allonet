#include <string.h>
#include <stdio.h>
#include <enet/enet.h>
#include <assert.h>

#include <allonet/allonet.h>
#include "util.h"

static alloserver* serv;

static void send_interaction_to_client(alloserver* serv, alloserver_client* client, const char* from_entity, const char* to_entity, const char* cmd)
{
  cJSON* cmdrep = cjson_create_object(
    "cmd", cJSON_CreateString("interact"),
    "interact", cjson_create_object(
      "from_entity", cJSON_CreateString(from_entity ? from_entity : ""),
      "to_entity", cJSON_CreateString(to_entity ? to_entity : ""),
      "cmd", cJSON_CreateString(cmd),
      NULL
    ),
    NULL
  );
  const char* json = cJSON_Print(cmdrep);
  cJSON_Delete(cmdrep);

  serv->send(serv, client, CHANNEL_COMMANDS, json, strlen(json)+1);
  free((void*)json);
}

static void clients_changed(alloserver* serv, alloserver_client* added, alloserver_client* removed)
{

}

static void handle_intent(alloserver* serv, alloserver_client* client, allo_client_intent intent)
{
  client->intent = intent;
}

static void handle_interaction(alloserver* serv, alloserver_client* client, allo_interaction *interaction)
{
  
}

static void received_from_client(alloserver* serv, alloserver_client* client, allochannel channel, const uint8_t* data, size_t data_length)
{
  cJSON* cmd = cJSON_Parse(data);
  if (channel == CHANNEL_STATEDIFFS)
  {
    const cJSON* ntvintent = cJSON_GetObjectItem(cmd, "intent");
    allo_client_intent intent = allo_client_intent_parse_cjson(ntvintent);
    handle_intent(serv, client, intent);
  }
  else if (channel == CHANNEL_COMMANDS)
  {
    allo_interaction* interaction = allo_interaction_parse_cjson(cmd);
    handle_interaction(serv, client, interaction);
    allo_interaction_free(interaction);
  }
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
  const char* json = cJSON_Print(map);
  cJSON_Delete(map);

  int jsonlength = strlen(json);

  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    serv->send(serv, client, CHANNEL_STATEDIFFS, json, jsonlength + 1);
  }
}

static void step(double dt)
{
  while (serv->interbeat(serv, 1)) {}

  allo_client_intent intents[32];
  int count = 0;
  alloserver_client* client;
  LIST_FOREACH(client, &serv->clients, pointers) {
    intents[count++] = client->intent;
    if (count == 32) break;
  }
  allo_simulate(&serv->state, dt, intents, count);
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
        sleep(1);
      }
      else {
        fprintf(stderr, "giving up. Is another server running?\n");
        break;
      }
    }
  }

  if (!serv) {
    perror("errno");
    return -2;
  }
  serv->clients_callback = clients_changed;
  serv->raw_indata_callback = received_from_client;
  LIST_INIT(&serv->state.entities);

  fprintf(stderr, "alloserv_run_standalone open\n");
  int allosocket = allo_socket_for_select(serv);

  while (1) {
    ENetSocketSet set;
    ENET_SOCKETSET_EMPTY(set);
    ENET_SOCKETSET_ADD(set, allosocket);

    int selectr = enet_socketset_select(allosocket, &set, NULL, 10);
    if (selectr < 0) {
      perror("select failed, terminating");
      return -3;
    }
    else
    {
      step(0.01);
    }
  }

  return 0;
}