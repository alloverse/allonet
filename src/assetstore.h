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
    
    /// Reads some data from an asset
    /// @param store The asset store
    /// @param asset_id The asset to read from
    /// @param offset Byte offset to start reading from
    /// @param data The data buffer to read into
    /// @param length The size of the data buffer and max length to read
    /// @param out_total_size Is set to the total size of the asset.
    /// @returns The number of bytes read, or a negative value on error.
    /// @note Trying to read bytes from an incomplete asset may return ininitialized data. Check with `assetstore_state`.
    int (*read)(struct assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size);

    /// Writes some data to an asset
    /// @param store The asset store
    /// @param asset_id The asset to read from
    /// @param offset Byte offset to start writing at
    /// @param data The data to write
    /// @param length The size of the data to write
    /// @param total_size The expected total size of the asset.
    /// @returns Negative error code, or positive number of bytes written
    int (*write)(struct assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size);

    /// Return 1 if asset is complete.
    int (*get_is_asset_complete)(struct assetstore *store, const char *asset_id);

    /// Get state of an asset
    /// @param store The store
    /// @param asset_id The asset
    /// @param out_exists Optional. Will be set to 1 if the asset exists
    /// @param out_complete Optional. Will be set to 1 if the all of the assets bytes are stored
    /// @param out_ranges_count Optional. The number of missing ranges for the asset. Get the ranges with `assetstore_get_missing_ranges`
    /// @returns 0 on success
    int (*get_state)(struct assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count);

    /// Get missing ranges for an asset
    /// @param store The store
    /// @param asset_id The asset
    /// @param out_ranges Where to put the ranges on the format [offset, length]. Must fit `size_t * count * 2`
    /// @param count The number of ranges that fits in `out_ranges`
    /// @returns The number of ranges added to `out_ranges`
    size_t (*get_missing_ranges)(struct assetstore *store, const char *asset_id, size_t out_ranges[], size_t count);

    /// Register an asset with the store
    int (*register_asset)(struct assetstore *store, const char *name);

    /// Get a file path for an asset to be used where streaming data is not possible.
    /// Stores not backed by disk need to write to a temp file and provide the path for it.
    /// @returns the path on success, otherwise NULL. The pointer should be freed by the caller.
    char *(*get_asset_file_path)(struct assetstore *store, const char *asset_id);
    
    void *_impl;
} assetstore;

typedef struct asset_diskstore {
    assetstore *interface;
    
    /// The disk root
    const char *disk_path;
    /// State tracking, completed ranges etc
    cJSON *state;
    
    mtx_t lock;
    int refcount;
} asset_diskstore;

int asset_diskstore_init(assetstore *store, const char *disk_path);
void asset_diskstore_deinit(assetstore *store);

#endif /* asset_store_h */
