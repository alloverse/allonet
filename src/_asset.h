//
//  _asset.h
//  allonet
//
//  Created by Patrik on 2020-11-25.
//

#ifndef _asset_h
#define _asset_h

// Asset channel message types
typedef enum asset_mid {
    asset_mid_request = 1,
    asset_mid_data = 2,
    asset_mid_failure = 3
} asset_mid;

/// Asset error response types
typedef enum asset_error_code {
    asset_not_available_error = 1,
    asset_malformed_request_error = 2,
    asset_file_read_error = 3,
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

#endif /* _asset_h */
