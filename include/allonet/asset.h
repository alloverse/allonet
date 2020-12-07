//
//  asset.h
//  allonet
//
//  Created by Patrik on 2020-06-12.
//

#ifndef asset_h
#define asset_h

typedef struct assetstore assetstore;
typedef enum asset_mid asset_mid;

/// Provide assets a way to send data in the network
/// @param mid The message id
/// @param header The message header
/// @param data The data to send
/// @param data_length The length of `data`
/// @param user The same pointer as sent to a method that also takes this function
typedef void(*asset_send_func)(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, const void *user);

/// Provide assets a way to process infoming data
/// @param data The received data to be processed
/// @param data_length The size of the `data` buffer
/// @param read_range A function to read asset data from storage
/// @param write_range A function to write asset data to storage
/// @param send A function to send data over the network
/// @param user Passed to the `read_range`, `write_range` and `send` methods.
void asset_handle(
    const uint8_t* data,
    size_t data_length,
    assetstore *store,
    asset_send_func send,
    const void *user
);

/// Make an asset request
/// @param id The asset id to request
/// @param entity_id Optional id of the entity that needs the asset
/// @param send A function to send data over the network
/// @param user Passed to the `read_range`, `write_range` and `send` methods.
void asset_request(
    const char *id,
    const char *entity_id,
    asset_send_func send,
    void *user
);




// Protocol details


/// The protocol message header of asset protocol
typedef struct asset_packet_header {
    /// Message identifier
    uint16_t mid;
    /// Header size
    uint16_t hlen;
} asset_packet_header;


/// Asset channel message types
typedef enum asset_mid {
    /// Asset request
    asset_mid_request = 1,
    /// Asset request success response
    asset_mid_data = 2,
    /// Asset request failure response
    asset_mid_failure = 3
} asset_mid;

struct _ENetPacket;
/// Helper method to pack an asset into an enet package
/// @param mid The message id
/// @param header The message specific header
/// @param data Data to pack. Copied into the resulting packet
/// @param data_length The size of the `data` buffer
struct _ENetPacket *asset_build_enet_packet(uint16_t mid, const cJSON *header, const uint8_t *data, size_t data_length);

#endif /* asset_h */
