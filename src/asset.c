#include "client/_client.h"
#include <stdio.h>
#include <allonet/asset.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "util.h"

#define min(a, b) a < b ? a : b;

/// Reads the asset header and adjusts the data and data_length pointers to match remaining data after the header
/// Puts the json header in `out_json` which must then be `cJSON_free`'d
/// returns 0 on success
int asset_read_header(uint8_t const **data, size_t *data_length, uint16_t *out_mid, cJSON const **out_json) {
    assert(data);
    assert(data_length);
    assert(out_json);
    assert(out_mid);
    
    uint16_t mid = ntohs(*(uint16_t*)(*data));
    *data += sizeof(uint16_t);
    *data_length -= sizeof(uint16_t);
    
    uint16_t header_len = ntohs(*(uint16_t*)(*data));
    *data += sizeof(uint16_t) + header_len;
    *data_length -= sizeof(uint16_t) - header_len;
    
    const char *parse_end;
    *out_json = cJSON_ParseWithOpts((char *)*data, &parse_end, false);
    size_t parsed_len = (size_t)parse_end - (size_t)*data;
    assert(parsed_len == header_len);
    *out_mid = mid;
    
    return 0;
}

int asset_write(cJSON *header, const uint8_t *data, size_t data_length) {
    return 0;
}


void parse_asset(alloclient *client, const char *data, int length) {
    
}


void asset_send_work(asset_job *job) {
    // Network can accept more data?
    int count_max = min(job->network_max_accept_bytes, job->buffer_size);
    
    // Request data from host
    int len = job->request_data(job->asset_id, job->buffer, job->offset, count_max);
    
    // Send the data to the network
    if (len > 0) {
        job->send_data(job->buffer, len);
    } else {
        printf("no data read");
    }
}

