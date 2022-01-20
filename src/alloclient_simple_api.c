//
//  alloclient_simple_api.c
//  allocubeappliance
//
//  Created by Patrik Sjöberg on 2022-01-18.
//

#include "alloclient_simple_api.h"
#include <allonet/arr.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "client/_client.h"
#include "util.h"
#include "clientproxy.h"
#include "asset.h"
#include <allonet/assetstore.h>

void push_event(cJSON *jevent);

void _disconnected(alloclient *client, alloerror code, const char *message) {
    push_event(cjson_create_object("event", cJSON_CreateString("disconnected"),
                                   "code", cJSON_CreateNumber(code),
                                   "message", cJSON_CreateString(message),
                                   NULL));
}

void _clock(alloclient *client, double latency, double delta) {
    push_event(cjson_create_object("event", cJSON_CreateString("clock"),
                                   "latency", cJSON_CreateNumber(latency),
                                   "delta", cJSON_CreateNumber(delta),
                                   NULL));
}

void _state(alloclient *client, allo_state *state) {
    cJSON *json = allo_state_to_json(state, true);
    push_event(cjson_create_object("event", cJSON_CreateString("state"),
                                   "state", json,
                                   NULL));
}

bool _interaction(alloclient *client, allo_interaction *interaction) {
    cJSON *json = allo_interaction_to_cjson(interaction);
    push_event(cjson_create_object("event", cJSON_CreateString("interaction"),
                                   "interaction", json,
                                   NULL));
    return true;
}

void _asset_receive(alloclient *client, char *name, uint8_t *bytes, size_t offset, size_t length, size_t total_size) {
    push_event(cjson_create_object("event", cJSON_CreateString("asset_receive"),
                                   "name", cJSON_CreateString(name),
                                   "offset", cJSON_CreateNumber(offset),
                                   "length", cJSON_CreateNumber(length),
                                   "total_size", cJSON_CreateNumber(total_size),
                                   NULL));
}

enum {
    JOP_INVALID = -1,
    JOP_POLL,
    JOP_CONNECT,
    JOP_INTERACTION,
    JOP_INTENT
};

int jopcode(cJSON *jop) {
    if (jop == NULL)
        return JOP_INVALID;
    if (cJSON_IsString(jop)) {
        char *op = cJSON_GetStringValue(jop);
        if (strcmp(op, "poll") == 0) {
            return JOP_POLL;
        } else if (strcmp(op, "connect") == 0) {
            return JOP_CONNECT;
        } else if (strcmp(op, "interaction") == 0) {
            return JOP_INTERACTION;
        } else if (strcmp(op, "intent") == 0) {
            return JOP_INTENT;
        }
    } else {
        return (int)cJSON_GetNumberValue(jop);
    }
    return JOP_INVALID;
}

static struct alloclient_simple {
    alloclient *client;
    char *command_result_buffer;
    cJSON *events;
} _simple;

void push_event(cJSON *jevent) {
    if (_simple.events == NULL) {
        _simple.events = cJSON_CreateArray();
    }
    
    cJSON_AddItemToArray(_simple.events, jevent);
}

int alloclient_simple_init(int threaded) {
    if (_simple.client != NULL) return 1;
    allo_initialize(false);
    _simple.client = alloclient_create(threaded);
    
    _simple.client->disconnected_callback = _disconnected;
    _simple.client->clock_callback = _clock;
    _simple.client->state_callback = _state;
    _simple.client->interaction_callback = _interaction;
    return 1;
}

int alloclient_simple_free() {
    return 1;
}



char *alloclient_simple_communicate(const char * const command) {
    alloclient *client = _simple.client;
    cJSON *result = cJSON_CreateObject();
    cJSON *jcommand = cJSON_Parse(command);
    if (jcommand == NULL) {
        cJSON_AddItemToObject(result, "error", cjson_create_object("message", cJSON_CreateString("Malformed command"), "command", cJSON_CreateString(command), NULL));
        printf("Malformed command\n%s\n", command);
        goto bail;
    }
    cJSON *jop = cJSON_GetObjectItemCaseSensitive(jcommand, "op");
    int op = jopcode(jop);
    switch (op) {
        case JOP_INVALID:
            printf("Unknown code\n");
            break;
        case JOP_POLL: {
            int timeout = 100;
            cJSON *jtimeout = cJSON_GetObjectItemCaseSensitive(jcommand, "timeout");
            if (jtimeout && cJSON_IsNumber(jtimeout))
                timeout = (int)cJSON_GetNumberValue(jtimeout);
            alloclient_poll(client, timeout);
        } break;
        case JOP_CONNECT: {
            char *url = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jcommand, "url"));
            char *identity = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jcommand, "identity"));
            char *avatar_spec = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jcommand, "avatar_spec"));
            int ret = alloclient_connect(client, url, identity, avatar_spec);
            cJSON_AddItemToObject(result, "return", cJSON_CreateBool(ret));
        } break;
        case JOP_INTERACTION: {
            cJSON *jinteraction = cJSON_GetObjectItemCaseSensitive(jcommand, "interaction");
            allo_interaction *interaction = allo_interaction_parse_cjson(jinteraction);
            alloclient_send_interaction(client, interaction);
            allo_interaction_free(interaction);
        } break;
        case JOP_INTENT: {
            cJSON *jintent = cJSON_GetObjectItemCaseSensitive(jcommand, "intent");
            allo_client_intent *intent = allo_client_intent_parse_cjson(jintent);
            alloclient_set_intent(client, intent);
            allo_client_intent_free(intent);
        } break;
        default:
            printf("Unhandled jop '%s'\n", command);
            break;
    }
    
bail:
    if (_simple.events) {
        cJSON_AddItemToObject(result, "events", _simple.events);
    }
    if (_simple.command_result_buffer)
        free(_simple.command_result_buffer);
    _simple.command_result_buffer = cJSON_Print(result);
    cJSON_free(result);
    _simple.events = NULL;
    return _simple.command_result_buffer;
}
