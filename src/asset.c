#include "client/_client.h"
#include <stdio.h>
#include <allonet/asset.h>
#include <allonet/assetstore.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "_asset.h"

#define min(a, b) a < b ? a : b;

///
/// asset_id: asset id
/// error_code: computer-reladable error code for this error
/// error_reason: user-readable unavailability reason
cJSON *asset_error(const char *asset_id, asset_error_code error_code, char *error_reason) {
    assert(asset_id);
    assert(error_reason);
    assert(error_code);
    return cjson_create_object(
        "id", cJSON_CreateString(asset_id),
        "error_reason", cJSON_CreateString(error_reason),
        "error_code", cJSON_CreateNumber((double)error_code),
        NULL
    );
}

int asset_read_request_header(const cJSON *header, const char **out_id, size_t *out_start, size_t *out_length, cJSON **out_error_response) {
    // https://github.com/alloverse/docs/blob/master/specifications/assets.md#csc-asset-request
    cJSON *id = cJSON_GetObjectItem(header, "id");
    if (cJSON_IsString(id)) {
        *out_id = id->valuestring;
    } else {
        *out_error_response = asset_error("", asset_malformed_request_error, "The 'id' field must be a string");
        return asset_malformed_request_error;
    }
    
    cJSON *range = cJSON_GetObjectItem(header, "range");
    if (!cJSON_IsArray(range)) {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    if (cJSON_GetArraySize(range) != 2) {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    cJSON *start = cJSON_GetArrayItem(range, 0);
    if (cJSON_IsNumber(start)) {
        *out_start = start->valueint;
    } else {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    cJSON *length = cJSON_GetArrayItem(range, 1);
    if (cJSON_IsNumber(length)) {
        *out_length = length->valueint;
    } else {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    return 0;
}


int asset_read_data_header(const cJSON *header, const char **out_id, size_t *out_start, size_t *out_length, size_t *out_total_length, cJSON **out_error_response) {
    //https://github.com/alloverse/docs/blob/master/specifications/assets.md#csc-asset-response-transmission-header
    cJSON *id = cJSON_GetObjectItem(header, "id");
    if (cJSON_IsString(id)) {
        *out_id = id->valuestring;
    } else {
        *out_error_response = asset_error("", asset_malformed_request_error, "The 'id' field must be present and be a string");
        return asset_malformed_request_error;
    }
    
    cJSON *range = cJSON_GetObjectItem(header, "range");
    if (!cJSON_IsArray(range)) {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be present and be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    if (cJSON_GetArraySize(range) != 2) {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    cJSON *start = cJSON_GetArrayItem(range, 0);
    if (cJSON_IsNumber(start)) {
        *out_start = start->valueint;
    } else {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    cJSON *length = cJSON_GetArrayItem(range, 1);
    if (cJSON_IsNumber(length)) {
        *out_length = length->valueint;
    } else {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'range' field must be an array with exactly two integers");
        return asset_malformed_request_error;
    }
    
    
    cJSON *total_length = cJSON_GetObjectItem(header, "total_length");
    if (cJSON_IsNumber(total_length)) {
        *out_total_length = total_length->valueint;
    } else {
        *out_error_response = asset_error(id->string, asset_malformed_request_error, "The 'total_length' field must be present and a number");
        return asset_malformed_request_error;
    }
    
    return 0;
}

/// Does all the work with a package from the asset data channel, via function pointers provided
void asset_handle(
    const uint8_t* data,
    size_t data_length,
    assetstore *store,
    asset_send_func send,
    const void *user
) {
    uint16_t mid = 0;
    cJSON *json = NULL;
    cJSON *error = NULL;
    assert(asset_read_header(&data, &data_length, &mid, &json) == 0);
    if (mid == asset_mid_request) {
        
        const char *id = NULL;
        size_t offset = 0, length = 0;
        
        if (asset_read_request_header(json, &id, &offset, &length, &error)) {
            send(asset_mid_failure, error, NULL, 0, user);
            cJSON_Delete(json);
            cJSON_Delete(error);
            return;
        }
        
        size_t total_size = 0;
        int read_length = 0;
        uint8_t *read_buffer = malloc(length);
        
        read_length = assetstore_read(store, id, offset, read_buffer, length);
        
        if (read_length < 0) {
            error = asset_error(id, read_length, "Failed to read");
            send(asset_mid_failure, error, NULL, 0, user);
            cJSON_Delete(json);
            cJSON_Delete(error);
            return;
        }
        
        int response_range[2] = {offset, read_length};
        cJSON *response = cjson_create_object(
            "id", cJSON_CreateString(id),
            "range", cJSON_CreateIntArray(response_range, 2),
            "total_length", cJSON_CreateNumber((double)total_size),
            NULL
        );
        
        send(asset_mid_data, response, read_buffer, read_length, user);
        
        free(read_buffer);
    } else if (mid == asset_mid_data) {
        printf("Asset: received data: %s\n", cJSON_Print(json));
        const char *id = NULL;
        size_t offset = 0, length = 0, total_length = 0;
        if(asset_read_data_header(json, &id, &offset, &length, &total_length, &error)) {
            cJSON_Delete(json);
            cJSON_Delete(error);
            return;
        }
        
        assetstore_write(store, id, offset, data, length, total_length);
        
    } else if (mid == asset_mid_failure) {
        //https://github.com/alloverse/docs/blob/master/specifications/assets.md#csc-asset-response-failure-header
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *reason = cJSON_GetObjectItem(json, "error_reason");
        cJSON *code = cJSON_GetObjectItem(json, "error_code");
        
        printf("Asset: received error: %s", cJSON_Print(json));
    }
    
    if (json != NULL) {
        cJSON_free(json);
    }
    
    if (error != NULL) {
        cJSON_free(error);
    }
}

void asset_request(
    const char *id,
    const char *entity_id,
    asset_send_func send,
    void *user
) {
    int range[2] = {0, 50};
    cJSON *header = cjson_create_object(
        "id", cJSON_CreateString(id),
        "range", cJSON_CreateIntArray(range, 2),
        NULL
    );
    if (entity_id) {
        cJSON_AddStringToObject(header, "published_by", entity_id);
    }
    send(asset_mid_request, header, NULL, 0, user);
}

int asset_read_header(uint8_t const **data, size_t *data_length, uint16_t *out_mid, cJSON **out_json) {
    assert(data);
    assert(data_length);
    assert(out_json);
    assert(out_mid);
    
    uint16_t mid = ntohs(*(uint16_t*)(*data));
    *data += sizeof(uint16_t);
    *data_length -= sizeof(uint16_t);
    
    uint16_t header_len = ntohs(*(uint16_t*)(*data));
    *data += sizeof(uint16_t);
    *data_length -= sizeof(uint16_t);
    
    const char *parse_end;
    *out_json = cJSON_ParseWithOpts((char *)*data, &parse_end, false);
    size_t parsed_len = (size_t)(parse_end - (char*)(*data));
    assert(parsed_len == header_len);
    *out_mid = mid;
    
    
    *data += header_len;
    *data_length -= header_len;
    
    return 0;
}

ENetPacket *asset_build_enet_packet(uint16_t mid, const cJSON *header, const uint8_t *data, size_t data_length) {
    
    const char *json = cJSON_Print(header);
    size_t jsonlength = strlen(json);
    
    asset_packet_header h;
    h.mid = htons(mid);
    h.hlen = htons(jsonlength);
    
    ENetPacket *packet = enet_packet_create(NULL, sizeof(asset_packet_header) + jsonlength + data_length, ENET_PACKET_FLAG_RELIABLE);
    memcpy(packet->data, &h, sizeof(asset_packet_header));
    memcpy(packet->data + sizeof(asset_packet_header), json, jsonlength);
    memcpy(packet->data + sizeof(asset_packet_header) + jsonlength, data, data_length);
    
    free((void*)json);
    return packet;
}

