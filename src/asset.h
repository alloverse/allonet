//
//  asset.h
//  allonet
//
//  Created by Patrik on 2020-06-12.
//

#ifndef asset_h
#define asset_h

// types

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

typedef enum {
    /// Asset became available.
    asset_state_now_available,
    /// Asset became unavailable.
    asset_state_now_unavailable,
    /// Asset was requested but was not available.
    asset_state_requested_unavailable,
    /// Asset handling is not supported.
    asset_state_not_supported,
} asset_state;

/// Provide assets a way to send data in the network
/// @param mid The message id
/// @param header The message header
/// @param data The data to send
/// @param data_length The length of `data`
/// @param user The same pointer as sent to a method that also takes this function
typedef void(*asset_send_func)(asset_mid mid, const cJSON *header, const uint8_t *data, size_t data_length, void *user);

/// A chunk of data was requested. Deliver the data immediately or later using the `asset_deliver_bytes` method
typedef bool (*asset_request_func)(const char *asset_id, size_t offset, size_t length, void *user);

/// Write a chunc of data
typedef int (*asset_write_func)(const char *asset_id, const uint8_t *buffer, size_t offset, size_t length, size_t total_size, void *user);



/// A callback for when the state of an asset changes, for example when asset availability changes.
typedef void(*asset_state_func)(const char *asset_id, asset_state state, void *user);

/// Provide assets a way to process infoming data
/// @param data The received data to be processed
/// @param data_length The size of the `data` buffer
/// @param send A function to send data over the network
/// @param callback A function that gets called when the state of an asset changes.
/// @param user Passed to the `read_range`, `write_range` and `send` methods.
void asset_handle(
    const uint8_t* data,
    size_t data_length,
    asset_request_func request,
    asset_write_func write,
    asset_send_func send,
    asset_state_func callback,
    void *user
);

/// Make an asset request
/// @param id The asset id to request
/// @param entity_id Optional id of the entity that needs the asset
/// @param send A function to send data over the network
/// @param user Passed to `send`.
void asset_request(
    const char *asset_id,
    const char *entity_id,
    asset_send_func send,
    void *user
);

/// Start delivering an asset.
/// @param id The asset to deliver
/// @param store The asset store
/// @param request A callback for requesting data. Deliver using `asset_deliver_bytes`
/// @param send A function to send data over the network
/// @param user User data passed as last argument to callbacks
void asset_deliver(const char *asset_id, asset_request_func request, asset_send_func send, void *user);

/// Deliver bytes after receiving a `asset_request_func`.
/// @param asset_id The asset
/// @param data The data to deliver
/// @param offset The offset. Must correspond to the offset in the request
/// @param length The number of bytes in data
/// @param total_size The total size of the asset
/// @param send Callback to use to send data
/// @param user User data passed as the last argument of `send`
///
void asset_deliver_bytes(const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size, asset_send_func send, void *user);


/// Generates an identifier for the supplied bytes.
/// @returns the identifier. Free the result when you are done.
char *asset_generate_identifier(const uint8_t *bytes, size_t size);

// Protocol details



struct _ENetPacket;
/// Helper method to pack an asset into an enet package
/// @param mid The message id
/// @param header The message specific header
/// @param data Data to pack. Copied into the resulting packet
/// @param data_length The size of the `data` buffer
struct _ENetPacket *asset_build_enet_packet(uint16_t mid, const cJSON *header, const uint8_t *data, size_t data_length);



/// Asset error response types
typedef enum asset_error_code {
    asset_not_available_error = 1,
    asset_malformed_request_error = 2,
} asset_error_code;

/// Safe parsing of the request message header
/// `out_id` will point to data in `header` and only valid as long as header is.
/// 0 is returned on success. On failure an `asset_error_code` is returned and `out_error_response` will be an error response header
/// that should be sent back to the sender.
/// @param header The header to read
/// @param out_id The asset id from the header
/// @param out_start The requested starting byte offset
/// @param out_length The requested length
/// @param out_error_response Eventual error response header to send back to the requesting sender.
int asset_read_request_header(const cJSON *header, const char **out_id, size_t *out_start, size_t *out_length, cJSON **out_error_response);

/// Reads the asset header and adjusts the data and data_length pointers to match remaining data after the header
/// Puts the json header in `out_json` which must then be `cJSON_free`'d
/// returns 0 on success
/// @param data Raw data received. Must be a full data packet
/// @param data_length The length of data.
/// @param out_mid To store the message id
/// @param out_json To store the header
int asset_read_header(uint8_t const **data, size_t *data_length, uint16_t *out_mid, cJSON **out_json);


#endif /* asset_h */
