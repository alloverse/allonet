//#define _XOPEN_SOURCE 500
#include <string.h>
#include <cJSON/cJSON.h>
#include <unity.h>
#include <allonet/jobs.h>
#include <stdlib.h>
#include <allonet/arr.h>
#include <stdbool.h>
#include <memory.h>
#include <stdarg.h>
#include <ftw.h>
#include <assert.h>
#include "../src/asset.h"
#include <allonet/assetstore.h>


static bool didCallRequest;
static char *requestedAssetId;

void setUp() {
    didCallRequest = false;
    requestedAssetId = NULL;
}

void tearDown() {
    if (requestedAssetId != NULL) {
        free(requestedAssetId);
    }
}

typedef struct {
    bool didCallRequestMethod;
    char *requestedAssetId;
    size_t requestedOffset;
    size_t requestedLength;
    
    char *respondWithText;
    
    bool didCallSendMethod;
    asset_mid sentMessageType;
    cJSON *sentJsonHeader;
    uint8_t *sentData;
    size_t sentDataLength;
    
    bool didCallWriteMethod;
    char *writtenAssetId;
    uint8_t *writtenData;
    size_t writtenDataOffset;
    size_t writtenDataLength;
    size_t writtenDataTotalSize;
    
    bool didCallStateCallbac;
    char *stateAssetId;
    asset_state state;
} test_context;

void clean_test_context(test_context *context) {
    if (context->requestedAssetId) {
        free(context->requestedAssetId);
        context->requestedAssetId = NULL;
    }
    
    if (context->sentData) {
        free(context->sentData);
        context->sentData = NULL;
    }
    
    if (context->sentJsonHeader) {
        cJSON_Delete(context->sentJsonHeader);
        context->sentJsonHeader = NULL;
    }
    
    if (context->writtenAssetId) {
        free(context->writtenAssetId);
        context->writtenAssetId = NULL;
    }
    
    if (context->writtenData) {
        free(context->writtenData);
        context->writtenData = NULL;
    }
    
    if (context->stateAssetId) {
        free(context->stateAssetId);
        context->stateAssetId = NULL;
    }
}

/// Provide assets a way to send data in the network
/// @param mid The message id
/// @param header The message header
/// @param data The data to send
/// @param data_length The length of `data`
/// @param user The same pointer as sent to a method that also takes this function
static void __asset_send_func(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user) {
    test_context *context = (test_context *)user;
    printf("send was called\n");
    context->didCallSendMethod = true;
    context->sentData = malloc(data_length);
    memcpy(context->sentData, data, data_length);
    context->sentDataLength = data_length;
    context->sentJsonHeader = cJSON_Duplicate(header, true);
    context->sentMessageType = mid;
}

/// Request a chunk of data
/// Return true if the requested chunk is available
static bool __asset_request_func(const char *asset_id, size_t offset, size_t length, void *user) {
    test_context *context = (test_context*)user;
    
    printf("request was called\n");
    context->didCallRequestMethod = true;
    context->requestedAssetId = strdup(asset_id);
    context->requestedOffset = offset;
    context->requestedLength = length;
    
    if (context->respondWithText) {
        char *response = context->respondWithText;
        asset_deliver_bytes(asset_id, (uint8_t*)response, offset, strlen(response), strlen(response), __asset_send_func, user);
        return 1;
    }
    return 0;
}

/// Write a chunc of data
static int __asset_write_func(const char *asset_id, const uint8_t *buffer, size_t offset, size_t length, size_t total_size, void *user) {
    test_context *context = (test_context *)user;
    printf("write was called\n");
    
    context->didCallWriteMethod = true;
    context->writtenData = malloc(length);
    memcpy(context->writtenData, buffer, length);
    context->writtenDataLength = length;
    context->writtenDataOffset = offset;
    context->writtenDataTotalSize = total_size;
    context->writtenAssetId = strdup(asset_id);
    return length;
}

/// A callback for when the state of an asset changes, for example when asset availability changes.
static void __asset_state_func(const char *asset_id, asset_state state, void *user) {
    printf("state was called\n");
    
}


void test_receive_request_respond_with_some_data() {
    //packet for an asset request is received
    //packet structure is
    // <message type:2><header_length:2><json_string:header_length><data:n>
    
    //asset request json structure:
    // {
    //     id: <asset identifier>,
    //     range: [<first byte>, <number of bytes>]
    // }
    // no data
    char *packet = "\0\1\0\x20{\"id\":\"abc123\", \"range\":[5, 20]}";
    
    test_context context = {0};
    context.respondWithText = "Hello, this is the asset";
    
    asset_handle((uint8_t*)packet, sizeof(packet), __asset_request_func, __asset_write_func, __asset_send_func, __asset_state_func, (void*)&context);
    
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abc123", context.requestedAssetId, "Should call request callback with the asset id of the request");
    TEST_ASSERT_EQUAL(context.requestedLength, 20);
    TEST_ASSERT_EQUAL(context.requestedOffset, 5);
    
    TEST_ASSERT_TRUE_MESSAGE(context.didCallSendMethod, "Should send a reply")
    TEST_ASSERT_EQUAL(context.sentMessageType, asset_mid_data);
    TEST_ASSERT_EQUAL_STRING(cJSON_GetObjectItemCaseSensitive(context.sentJsonHeader, "id")->valuestring, "abc123");
    TEST_ASSERT_EQUAL_STRING(context.sentData, context.respondWithText);
    clean_test_context(&context);
}

void test_receive_all_data() {
    //packet for an asset request is received
    //packet structure is
    // <message type:2><header_length:2><json_string:header_length><data:n>
    
    //asset response header json structure:
    // {
    //     id: <asset identifier>,
    //     range: [<first byte>, <number of bytes>]
    // }
    // <data>
    char *packet = "\0\2\0\x34{\"id\":\"abc123\", \"range\":[0, 28], \"total_length\": 28}Hello, this is the data bits";
    
    test_context context = {0};
    
    asset_handle((uint8_t*)packet, sizeof(packet), __asset_request_func, __asset_write_func, __asset_send_func, __asset_state_func, (void*)&context);
    
    TEST_ASSERT_FALSE(context.didCallRequestMethod);
    TEST_ASSERT_FALSE(context.didCallSendMethod);
    TEST_ASSERT_TRUE(context.didCallWriteMethod);
    TEST_ASSERT_EQUAL_STRING(context.writtenData, "Hello, this is the data bits");
}

void test_receive_partial_data() {
    //packet for an asset request is received
    //packet structure is
    // <message type:2><header_length:2><json_string:header_length><data:n>
    
    //asset response header json structure:
    // {
    //     id: <asset identifier>,
    //     range: [<first byte>, <number of bytes>]
    // }
    // <data>
    char *packet = "\0\2\0\x34{\"id\":\"abc123\", \"range\":[0, 10], \"total_length\": 28}Hello, this is the data bits";
    
    test_context context = {0};
    
    asset_handle((uint8_t*)packet, sizeof(packet), __asset_request_func, __asset_write_func, __asset_send_func, __asset_state_func, (void*)&context);
    
    TEST_ASSERT_TRUE(context.didCallWriteMethod);
    TEST_ASSERT_EQUAL(context.writtenDataLength, 10);
    TEST_ASSERT_EQUAL(context.writtenDataOffset, 0);
    TEST_ASSERT_EQUAL(context.writtenDataTotalSize, 28);
    TEST_ASSERT_EQUAL_STRING_LEN("Hello, thi", context.writtenData, context.writtenDataLength);
    
    TEST_ASSERT_FALSE(context.didCallRequestMethod);
    
    TEST_ASSERT_TRUE_MESSAGE(context.didCallSendMethod, "Request for next range is sent");
    cJSON *range = cJSON_GetObjectItemCaseSensitive(context.sentJsonHeader, "range");
    TEST_ASSERT_EQUAL(context.writtenDataLength, cJSON_GetArrayItem(range, 0)->valueint);
    TEST_ASSERT_EQUAL(context.writtenDataTotalSize - context.writtenDataLength, cJSON_GetArrayItem(range, 1)->valueint);
}

void test_request_error() {
    //packet for an asset request is received
    //packet structure is
    // <message type:2><header_length:2><json_string:header_length><data:n>
    
    //asset error response header json structure:
    // {
    //     id: <asset identifier>,
    //     error_reason: <user readable string>,
    //     error_code: <error code number>
    // }
    // <no data>
    char *packet = "\0\2\0\x48{\"id\":\"abc123\", \"error_reason\":\"Something went wrong\", \"error_code\": 28}";
    
    test_context context = {0};
    
    asset_handle((uint8_t*)packet, sizeof(packet), __asset_request_func, __asset_write_func, __asset_send_func, __asset_state_func, (void*)&context);
    
    TEST_ASSERT_FALSE(context.didCallWriteMethod);
    TEST_ASSERT_FALSE(context.didCallRequestMethod);
    TEST_ASSERT_TRUE(context.didCallStateCallbac);
    TEST_ASSERT_EQUAL(asset_state_now_unavailable, context.state);
}

int main(void) {
    UNITY_BEGIN();

//    RUN_TEST(test_receive_request_respond_with_some_data);
//    RUN_TEST(test_receive_all_data);
//    RUN_TEST(test_receive_partial_data);
    RUN_TEST(test_request_error);
    return UNITY_END();
}
