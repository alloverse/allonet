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

/// Api for asset handling
typedef struct assetstore {
    
    /// Reads some data from an asset
    /// @param store The asset store
    /// @param asset_id The asset to read from
    /// @param offset Byte offset to start reading from
    /// @param data The data buffer to read into
    /// @param length The size of the data buffer and max length to read
    /// @param out_total_size Is set to the total size of the asset.
    /// @return The number of bytes read, or a negative value on error.
    /// @note Trying to read bytes from an incomplete asset may return ininitialized data. Check with `assetstore_state`.
    int (*read)(struct assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size);

    /// Writes some data to an asset
    /// @param store The asset store
    /// @param asset_id The asset to read from
    /// @param offset Byte offset to start writing at
    /// @param data The data to write
    /// @param length The size of the data to write
    /// @param total_size The expected total size of the asset.
    /// @return Negative error code, or positive number of bytes written
    int (*write)(struct assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size);

    /// Get state of an asset
    /// @param store The store
    /// @param asset_id The asset
    /// @param out_exists Optional. Will be set to 1 if the asset exists
    /// @param out_complete Optional. Will be set to 1 if the all of the assets bytes are stored
    /// @param out_ranges_count Optional. The number of missing ranges for the asset. Get the ranges with `assetstore_get_missing_ranges`
    /// @param out_size Optional. The assets size
    /// @return 0 on success
    int (*get_state)(struct assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count, size_t *out_size);

    /// Get missing ranges for an asset
    /// @param store The store
    /// @param asset_id The asset
    /// @param out_ranges Where to put the ranges on the format [offset, length]. Must fit `size_t * count * 2`
    /// @param count The number of ranges that fits in `out_ranges`
    /// @return The number of ranges added to `out_ranges`
    size_t (*get_missing_ranges)(struct assetstore *store, const char *asset_id, size_t out_ranges[], size_t count);
    
    void *_impl;
    
    cJSON *state;
    
    /// If 0, assetstore will only be used to track transfer states.
    int use_cache;
    /// Number of assets in the cache.
    size_t cache_count;
    /// Number of bytes in the cache.
    size_t cache_size;
    /// Cache pruning is done now and then. This tracks the next invocation.
    double next_state_prune;
    
    mtx_t lock;
} assetstore;


int assetstore_init(assetstore *store);
void assetstore_deinit(assetstore *store);

/// Reads some data from an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start reading from
/// @param data The data buffer to read into
/// @param length The size of the data buffer and max length to read
/// @param out_total_size Is set to the total size of the asset.
/// @returns The number of bytes read, or a negative value on error.
/// @note Trying to read bytes from an incomplete asset may return ininitialized data. Check with `assetstore_state`.
int assetstore_read(struct assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size);

/// Writes some data to an asset
/// @param store The asset store
/// @param asset_id The asset to read from
/// @param offset Byte offset to start writing at
/// @param data The data to write
/// @param length The size of the data to write
/// @param total_size The expected total size of the asset.
/// @returns Negative error code, or positive number of bytes written
int assetstore_write(struct assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size);

/// Get state of an asset
/// @param store The store
/// @param asset_id The asset
/// @param out_exists Optional. Will be set to 1 if the asset exists
/// @param out_complete Optional. Will be set to 1 if the all of the assets bytes are stored
/// @param out_ranges_count Optional. The number of missing ranges for the asset. Get the ranges with `assetstore_get_missing_ranges`
/// @param out_size Optional. The size of the asset.
/// @returns 0 on success
int assetstore_get_state(struct assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count, size_t *out_size);

/// Convenience method for `assetstore_get_state` only requesting `out_complete`
/// @return 1 if asset is complete.
int assetstore_get_is_asset_complete(struct assetstore *store, const char *asset_id);

/// Get missing ranges for an asset
/// @param store The store
/// @param asset_id The asset
/// @param out_ranges Where to put the ranges on the format [offset, length]. Must fit `size_t * count * 2`
/// @param count The number of ranges that fits in `out_ranges`
/// @returns The number of ranges added to `out_ranges`
size_t assetstore_get_missing_ranges(struct assetstore *store, const char *asset_id, size_t out_ranges[], size_t count);



typedef struct asset_memstore {
    struct assetstore *_interface;
} asset_memstore;

int asset_memstore_init(assetstore *store);
void asset_memstore_deinit(assetstore *store);
int asset_memstore_register_asset_nocopy(assetstore *store, const char *asset_id, const uint8_t *data, size_t length);
uint8_t *asset_memstore_get_data_pointer(assetstore *store, const char *asset_id);

#endif /* asset_store_h */
