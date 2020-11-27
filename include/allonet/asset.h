//
//  asset.h
//  allonet
//
//  Created by Patrik on 2020-06-12.
//

#ifndef asset_h
#define asset_h

/// Provide allonet way to read a range of data
typedef int(*asset_read_range_func)(const char *id, uint8_t *buffer, size_t offset, size_t length, size_t *out_read_length, size_t *out_total_size, cJSON **out_error, void *user);

/// Provide allonet a way to write a range of data
typedef int(*asset_write_range_func)(const char *id, const uint8_t *buffer, size_t offset, size_t length, cJSON **out_error, void *user);

typedef void(*asset_send_func)(uint16_t mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user);

void asset_handle(
    const uint8_t* data,
    size_t data_length,
    asset_read_range_func read_range,
    asset_write_range_func write_range,
    asset_send_func respond,
    void *user
);

void asset_request(
    const char *id,
    const char *entity_id,
    asset_send_func send,
    void *user
);


typedef struct asset_packet_header {
    /// Message identifier
    uint16_t mid;
    /// Header size
    uint16_t hlen;
} asset_packet_header;



//TODO: move to asset_enet.h?
struct _ENetPacket;
struct _ENetPacket *asset_build_enet_packet(uint16_t mid, const cJSON *header, const uint8_t *data, size_t data_length);

#endif /* asset_h */
