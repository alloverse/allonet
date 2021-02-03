#include "client/_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "asset.h"

#define min(a, b) a < b ? a : b;

// try to use a decent size. enets default max waiting data is 32Mb (ENET_HOST_DEFAULT_MAXIMUM_WAITING_DATA)
#define ASSET_CHUNK_SIZE 1024*1024

void _asset_request(
    const char *asset_id,
    const char *entity_id,
    size_t offset,
    size_t length,
    asset_send_func send,
    void *user
);

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

void asset_deliver_bytes(const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size, asset_send_func send, void *user) {
    
    if (data == NULL) {
        // ok user didn't have this chunk
        cJSON *error = asset_error(asset_id, asset_not_available_error, "Chunk not available");
        send(asset_mid_failure, error, NULL, 0, user);
        cJSON_Delete(error);
        return;
    }
    
    int response_range[2] = {offset, length};
    cJSON *response = cjson_create_object(
                                          "id", cJSON_CreateString(asset_id),
                                          "range", cJSON_CreateIntArray(response_range, 2),
                                          "total_length", cJSON_CreateNumber((double)total_size),
                                          NULL
                                          );
    
    send(asset_mid_data, response, data, length, user);
}

void asset_deliver(const char *asset_id, asset_request_func request, asset_send_func send, void *user) {
    assert(request);
    assert(send);
    
    if (request(asset_id, 0, ASSET_CHUNK_SIZE, user) == false) {
        cJSON *error = asset_error(asset_id, asset_not_available_error, "Can not deliver");
        send(asset_mid_failure, error, NULL, 0, user);
        cJSON_Delete(error);
    }
}

/// Does all the work with a package from the asset data channel, via function pointers provided
void asset_handle(
    const uint8_t* data,
    size_t data_length,
    asset_request_func request,
    asset_write_func write,
    asset_send_func send,
    asset_state_func callback,
    void *user
) {
    uint16_t mid = 0;
    cJSON *json = NULL;
    cJSON *error = NULL;
    
    if (asset_read_header(&data, &data_length, &mid, &json) != 0) {
        assert(false && "Failed to read header");
    }
    
    
    if (mid == asset_mid_request) {
        const char *asset_id = NULL;
        size_t offset = 0, length = 0;
        
        if (asset_read_request_header(json, &asset_id, &offset, &length, &error)) {
            send(asset_mid_failure, error, NULL, 0, user);
            cJSON_Delete(json);
            cJSON_Delete(error);
            return;
        }
        
        printf("Asset: Got a request for %s\n", asset_id);
        
        // If we can't read we just fail early.
        if (request == NULL) {
            printf("Asset: Asset reading not supported");
            callback(asset_id, asset_state_not_supported, user);
        } else if (request(asset_id, offset, length, user) == false) {
            callback(asset_id, asset_state_requested_unavailable, user);
        }
        // Then we wait for user to honor the request
    } else if (mid == asset_mid_data && write != NULL) { // if we can't write we just ignore the message
        printf("Asset: received data: %s\n", cJSON_Print(json));
        const char *asset_id = NULL;
        size_t offset = 0, length = 0, total_length = 0;
        if(asset_read_data_header(json, &asset_id, &offset, &length, &total_length, &error)) {
            cJSON_Delete(json);
            cJSON_Delete(error);
            return;
        }
        write(asset_id, data, offset, length, total_length, user);
        // request more?
        // TODO: check missing ranges instead
        if (offset + length < total_length) {
            _asset_request(asset_id, NULL, offset + length, length, send, user);
        } else {
            callback(asset_id, asset_state_now_available, user);
        }
        assert(offset + length <= total_length);
    } else if (mid == asset_mid_failure) {
        //https://github.com/alloverse/docs/blob/master/specifications/assets.md#csc-asset-response-failure-header
        
        printf("Asset: received error: %s", cJSON_Print(json));
    } else {
        printf("Asset: received weird mid: %d", mid);
    }
    
    if (json != NULL) {
        cJSON_free(json);
    }
    
    if (error != NULL) {
        cJSON_free(error);
    }
}


void _asset_request(
   const char *asset_id,
   const char *entity_id,
   size_t offset,
   size_t length,
   asset_send_func send,
   void *user
) {
    int range[2] = {offset, length};
    cJSON *header = cjson_create_object(
        "id", cJSON_CreateString(asset_id),
        "range", cJSON_CreateIntArray(range, 2),
        NULL
    );
    if (entity_id) {
        cJSON_AddStringToObject(header, "published_by", entity_id);
    }
    printf("Asset: Requesting %s\n", cJSON_Print(header));
    send(asset_mid_request, header, NULL, 0, user);
}


void asset_request(
    const char *asset_id,
    const char *entity_id,
    asset_send_func send,
    void *user
) {
    _asset_request(asset_id, entity_id, 0, ASSET_CHUNK_SIZE, send, user);
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

#include "sha1.h"

char *_asset_generate_identifier_sha1(const uint8_t *bytes, size_t size) {
    char sha[20] = {0};
    SHA1(sha, (const char *)bytes, size);
    
    char *prefix = "asset:sha1:";
    char *xsha = malloc(21 + strlen(prefix));
    memcpy(xsha, prefix, strlen(prefix));
    snprintf(xsha+strlen(prefix), 21, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", sha[0], sha[1], sha[2], sha[3], sha[4], sha[5], sha[6], sha[7], sha[8], sha[9], sha[10], sha[11], sha[12], sha[13], sha[14], sha[15], sha[16], sha[17], sha[18], sha[19]);

    return xsha;
}

#include "sha256.h"
char *_asset_generate_identifier_sha256(const uint8_t *bytes, size_t size) {
    uint8_t sha[SHA256_HASH_SIZE] = {0};
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, bytes, size);
    sha256_final(&ctx, sha);
    
    char *prefix = "asset:sha256:";
    char *xsha = malloc(SHA256_HASH_SIZE * 2 + strlen(prefix));
    memcpy(xsha, prefix, strlen(prefix));
    
    char *cursor = xsha + strlen(prefix);
    for (int i = 0; i < SHA256_HASH_SIZE; i++) {
        snprintf(cursor, SHA256_HASH_SIZE*2-i, "%02x", sha[i]);
        cursor += 2;
    }
    return xsha;
}


char *asset_generate_identifier(const uint8_t *bytes, size_t size) {
    return _asset_generate_identifier_sha256(bytes, size);
}

