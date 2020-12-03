//
//  asset_store.h
//  allonet
//
//  Created by Patrik on 2020-11-27.
//

#ifndef asset_store_h
#define asset_store_h

#include "inlinesys/queue.h"
#include <stdio.h>
#include <cJSON/cJSON.h>

typedef struct assetstore {
    
    /// The disk root
    const char *disk_path;
    /// State tracking, completed ranges etc
    cJSON *state;
    
} assetstore;

/// Open an assetstore
assetstore *assetstore_open(const char *disk_path);

/// Close and free an open assetstore
void assetstore_close(assetstore *store);


/// Get state of an asset in the store
/// @param store The store
/// @param asset_id The asset
/// @param out_exists Optional. Will be set to 1 if the asset exists
/// @param out_complete Optional. Will be set to 1 if the all of the assets bytes are stored
/// @param out_ranges_count Optional. The number of missing ranges for the asset. Get the ranges with `assetstore_get_missing_ranges`
/// @returns 0 on success
int assetstore_state(assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count);

/// Get missing ranges for an asset
/// @param start The range index to get
/// @param count The number of ranges from `start` to get, or 1 for only one.
/// @param out_ranges Where to put the ranges. Must fit `size_t * count * 2`
/// @returns The number of ranges added to `out_ranges`
size_t assetstore_get_missing_ranges(assetstore *store, const char *asset_id, size_t start, size_t count, size_t *out_ranges);

/// Read some data from an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start reading from
/// @param data The data buffer to read into
/// @param length The size of the data buffer and max length to read
/// @returns The number of bytes read, or a negative value on error.
int assetstore_read(assetstore *store, const char *asset_id, size_t offset, u_int8_t *buffer, size_t length);

/// Write some data to an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start writing at
/// @param data The data to write
/// @param length The size of the data to write
/// @param total_size The expected total size of the asset.
/// @returns Negative error code, or positive number of bytes written
int assetstore_write(assetstore *store, const char *asset_id, size_t offset, const u_int8_t *data, size_t length, size_t total_size);

#endif /* asset_store_h */
