//
//  asset_store.h
//  allonet
//
//  Created by Patrik on 2020-11-27.
//

#ifndef asset_store_h
#define asset_store_h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <cJSON/cJSON.h>
#include <tinycthread.h>

typedef struct assetstore {
    
    /// The disk root
    const char *disk_path;
    /// State tracking, completed ranges etc
    cJSON *state;
    
    int (*read)(struct assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size);
    int (*write)(struct assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size);
    
    mtx_t lock;
    int refcount;
} assetstore;

void assetstore_init();
void assetstore_deinit();

/// Open an assetstore
assetstore *assetstore_open(const char *disk_path);

/// Close and free an open assetstore
void assetstore_close(assetstore *store);

/// Return 1 if asset is complete.
int assetstore_asset_is_complete(assetstore *store, const char *asset_id);

/// Get state of an asset
/// @param store The store
/// @param asset_id The asset
/// @param out_exists Optional. Will be set to 1 if the asset exists
/// @param out_complete Optional. Will be set to 1 if the all of the assets bytes are stored
/// @param out_ranges_count Optional. The number of missing ranges for the asset. Get the ranges with `assetstore_get_missing_ranges`
/// @returns 0 on success
int assetstore_state(assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count);

/// Get missing ranges for an asset
/// @param store The store
/// @param asset_id The asset
/// @param out_ranges Where to put the ranges on the format [offset, length]. Must fit `size_t * count * 2`
/// @param count The number of ranges that fits in `out_ranges`
/// @returns The number of ranges added to `out_ranges`
size_t assetstore_get_missing_ranges(assetstore *store, const char *asset_id, size_t out_ranges[], size_t count);

/// Read some data from an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start reading from
/// @param data The data buffer to read into
/// @param length The size of the data buffer and max length to read
/// @param out_total_size Is set to the total size of the asset.
/// @returns The number of bytes read, or a negative value on error.
/// @note Trying to read bytes from an incomplete asset may return ininitialized data. Check with `assetstore_state`.
int assetstore_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size);

/// Write some data to an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start writing at
/// @param data The data to write
/// @param length The size of the data to write
/// @param total_size The expected total size of the asset.
/// @returns Negative error code, or positive number of bytes written
int assetstore_write(assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size);

/// Imports and publishes a file
int assetstore_assimilate(assetstore *store, const char *path);

/// Get a file path for an asset, if it exists.
/// Stores not backed by disk should write to a temp file and provide a path to it.
/// @returns the path on success, otherwise NULL
char *assetstore_asset_path(assetstore *store, const char *asset_id);

#endif /* asset_store_h */
