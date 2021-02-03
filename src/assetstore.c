//
//  asset_store.c
//  allonet
//
//  Created by Patrik on 2020-11-27.
//
#include <allonet/arr.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <allonet/assetstore.h>
#include "util.h"

#define min(a,b) a < b ? a : b
#define max(a,b) a > b ? a : b
#define log(f_, ...) printf((f_), ##__VA_ARGS__)

// Minimum number of seconds between cache prune checks
static const double kCacheCheckInterval = 10.0;
// Minimum number of seconds after last use to keep an asset in the cache.
// Before this an asset will not leave the cache even if memory or count demands it
// This guards assets that are currently being sent or received from disappearing.
static const double kCacheMinAge = 5.0;
// Maximum number of seconds after last use to keep an asset in the cache.
// After being left untouched for this long the asset is always removed
static const double kCacheMaxAge = 60.0*10;
// Maximum number of assets to keep in the cache (not counting static assets)
// This is useful because lookup times gets bad after a while...
static const int kCacheMaxCount = 400;
// Maximum number of bytes to keep in the store.
static const size_t kCacheMaxBytes = 1024*1024*1024;


size_t _first(cJSON *range) {
    return (size_t)cJSON_GetArrayItem(range, 0)->valueint;
}

size_t _last(cJSON *range) {
    return (size_t)cJSON_GetArrayItem(range, 1)->valueint;
}

cJSON *_range(cJSON *ranges, int index) {
    return cJSON_GetArrayItem(ranges, index);
}


/// Returns missing ranges
/// @param ranges The ranges
/// @param total_size The total size of the asset
/// @param count_max Stop counting after this many hits
/// @param out_ranges To store found ranges on the format [start, end]. Must be at least `size_t * count_max * 2` large
int _missing_ranges(cJSON *ranges, size_t total_size, size_t *out_ranges, size_t count_max) {
    if (count_max == 0) return 0;
    
    cJSON *range;
    size_t low = 0;
    size_t count = 0;
    size_t i = 0;
    cJSON_ArrayForEach(range, ranges) {
        if (count >= count_max) return count;
        if (low < _first(range)) {
            if (out_ranges) {
                out_ranges[i++] = low;
                out_ranges[i++] = _first(range) - 1;
            }
            ++count;
        }
        low = _last(range) + 1;
    }
    if (low < total_size && count < count_max) {
        if (out_ranges) {
            out_ranges[i++] = low;
            out_ranges[i++] = total_size;
        }
        ++count;
    }
    return count;
}

size_t __missing_ranges_count(cJSON *ranges, size_t total_size) {
    return _missing_ranges(ranges, total_size, NULL, ULONG_MAX);
}

size_t _missing_ranges_count(assetstore *store, const char *asset_id) {
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    assert(cJSON_IsObject(state));
    // No missing ranges if asset is complete
    if (cJSON_IsTrue(cJSON_GetObjectItem(state, "complete"))) {
        return 0;
    }
    cJSON *ranges = cJSON_GetObjectItem(state, "ranges");
    assert(cJSON_IsArray(ranges));
    cJSON *j_top = cJSON_GetObjectItem(state, "total_size");
    assert(cJSON_IsNumber(j_top));
    return __missing_ranges_count(ranges, j_top->valueint);
}

int _memstore_state(assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count, size_t *out_total_size) {
    assert(asset_id);
    
    mtx_lock(&store->lock);
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    // check existance
    if (state == NULL) {
        if (out_exists) *out_exists = 0;
        if (out_complete) *out_complete = 0;
        mtx_unlock(&store->lock);
        return 0;
    } else {
        if (out_exists) *out_exists = 1;
    }
    assert(cJSON_IsObject(state));
    if (!cJSON_IsObject(state)) {
        cJSON_DeleteItemFromObject(store->state, asset_id);
        mtx_unlock(&store->lock);
        return 1;
    }
    
    if (out_regions_count) {
        *out_regions_count = _missing_ranges_count(store, asset_id);
    }
    
    cJSON *total_size = cJSON_GetObjectItemCaseSensitive(state, "total_size");
    if (out_total_size && cJSON_IsNumber(total_size)) {
        *out_total_size = (size_t)total_size->valueint;
    }
    
    if (out_complete) {
        cJSON *complete = cJSON_GetObjectItem(state, "complete");
        if (complete == NULL) {
            *out_complete = 0;
        } else {
            assert(cJSON_IsBool(complete));
            *out_complete = cJSON_IsTrue(complete);
        }
    }
    mtx_unlock(&store->lock);
    return 0;
}

size_t _memstore_get_missing_ranges(assetstore *store, const char *asset_id, size_t *out_ranges, size_t count) {
    
    assert(store);
    assert(asset_id);
    assert(out_ranges);
    
    if (count == 0) { return 0; }
    
    mtx_lock(&store->lock);
    
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    // check existance
    if (state == NULL) {
        mtx_unlock(&store->lock);
        return 0;
    }
    cJSON *_total_size = cJSON_GetObjectItem(state, "total_size");
    assert(cJSON_IsNumber(_total_size));
    size_t total_size = (size_t)_total_size->valueint;
    
    cJSON *ranges = cJSON_GetObjectItem(state, "ranges");
    assert(cJSON_IsArray(ranges));
    
    size_t result = _missing_ranges(ranges, total_size, out_ranges, count);
    if (out_ranges == NULL) {
        mtx_unlock(&store->lock);
        return result;
    }
    
    // Adjust internal [start,end] to public [offset, length] format.
    for (size_t i = 0; i < result; i++) {
        out_ranges[i+1] = out_ranges[i+1] - out_ranges[i];
    }
    mtx_unlock(&store->lock);
    return result;
}

/// Note that rages are inclusive; easier to reason about and there's no point storing a zero range.
void _merge_range(cJSON *ranges, size_t start, size_t end) {
    assert(start <= end);
    
    int ranges_count = cJSON_GetArraySize(ranges);
    if (ranges_count == 0) {
        int numbers[2] = {(int)start, (int)end };
        cJSON *r = cJSON_CreateIntArray(numbers, 2);
        cJSON_AddItemToArray(ranges, r);
        return;
    }
    
    int i = 0, low = 0, high = cJSON_GetArraySize(ranges) - 1;
    size_t value;
    cJSON *range;
    // find the index `i` where to insert the new range, based on start value
    while (low <= high) {
        i = (low + high) / 2;
        range = cJSON_GetArrayItem(ranges, i);
        assert(range);
        value = _first(range);
        if (value < start) {
            low = i + 1;
        } else if (value > start) {
            high = i - 1;
        } else {
            break;
        }
    }
    // We want to end at the range that is bigger, or at end of list
    if (start >= value) {
        ++i;
    }
    
    // Merge all ranges that intersects to the right
    // Can be many as the length is unconstrained
    range = cJSON_GetArrayItem(ranges, i);
    while (range != NULL && (end >= _first(range) - 1 && start <= _last(range) + 1)) {
        start = min(start, _first(range));
        end = max(end, _last(range));
        cJSON_DeleteItemFromArray(ranges, i);
        range = cJSON_GetArrayItem(ranges, i);
    }
    
    // Merge all intersecting to the left
    // Can be at most one as we sorted by start
    range = i > 0 ? _range(ranges, i-1) : NULL;
    if (range && start <= _last(range) + 1) {
        start = min(start, _first(range));
        end = max(end, _last(range));
        cJSON_DeleteItemFromArray(ranges, i-1);
        --i;
    }
    
    // Insert the new range
    int numbers[2] = {(int)start, (int)end };
    cJSON *r = cJSON_CreateIntArray(numbers, 2);
    cJSON_InsertItemInArray(ranges, i, r);
    return;
}
void _lru_prune(assetstore *store);
void _update_asset_state(assetstore *store, const char *asset_id) {
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *ranges = cJSON_GetObjectItem(state, "ranges");
    
    // if we have exactly 1 range that is 0...total_size:
    // TODO: confirm that the file sha is equal to the asset_id
    // then we have the complete file
    cJSON *total_size = cJSON_GetObjectItem(state, "total_size");
    if (cJSON_GetArraySize(ranges) == 1) {
        cJSON *range = _range(ranges, 0);
        if (_first(range) == 0 && _last(range) == (size_t)total_size->valueint) {
            cJSON_ReplaceItemInObjectCaseSensitive(state, "complete", cJSON_CreateBool(1));
            cJSON_DeleteItemFromObject(state, "ranges");
            log("assetstore: Asset %s is complete\n", asset_id);
        }
    }
}

// clears allocated data
void _clear_state(cJSON *state) {
    if (cJSON_IsObject(state)) {
        cJSON *file = cJSON_GetObjectItem(state, "data");
        if (cJSON_IsRaw(file)) {
            free(file->valuestring);
        }
        if (cJSON_IsString(file)) {
            // warning: hacky hack-hack
            uint8_t *data = (uint8_t *)strtol(file->valuestring, NULL, 10);
            free(data);
        }
    }
}

void _lru_tag(cJSON *state) {
    double mono = get_ts_monod();
    cJSON *lru = cJSON_GetObjectItem(state, "lru");
    if (cJSON_IsNumber(lru)) {
        lru->valuedouble = mono;
    } else if (lru == NULL) {
        cJSON_AddNumberToObject(state, "lru", mono);
    } // lru can also be a bool:true, which means never prune
}

int _lru_compare_size(const void *a, const void *b) {
    double x = cJSON_GetObjectItemCaseSensitive((cJSON*)(*(cJSON**)a), "total_size")->valueint;
    double y = cJSON_GetObjectItemCaseSensitive((cJSON*)(*(cJSON**)b), "total_size")->valueint;
    return y - x;
}
int _lru_compare_age(const void *a, const void *b) {
    double x = cJSON_GetObjectItemCaseSensitive((cJSON*)(*(cJSON**)a), "lru")->valuedouble;
    double y = cJSON_GetObjectItemCaseSensitive((cJSON*)(*(cJSON**)b), "lru")->valuedouble;
    return x - y;
}

void _lru_prune(assetstore *store) {
    double time = get_ts_monod();
    
    if (store->use_cache == 0) {
        return;
    }
    
    int check_count = store->cache_count > kCacheMaxCount;
    int check_size = store->cache_size > kCacheMaxBytes;
    
    if (store->next_state_prune < time || check_count || check_size) {
        store->next_state_prune = time + kCacheCheckInterval;
    } else {
        return;
    }
    
    double max_barrier = time - kCacheMaxAge;
    cJSON *state = store->state->child;
    int total_asset_count = 0;
    int static_asset_count = 0;
    size_t total_memory_usage = 0;
    size_t static_memory_usage = 0;
    int pruned_old_count = 0;
    size_t pruned_old_size = 0;
    int pruned_max_count = 0;
    size_t pruned_max_size = 0;
    int pruned_memory_count = 0;
    size_t pruned_memory_size = 0;
    
    // Prune too old entries
    while (state) {
        cJSON *next = state->next;
        cJSON *lru = cJSON_GetObjectItemCaseSensitive(state, "lru");
        size_t size = cJSON_GetObjectItemCaseSensitive(state, "total_size")->valueint;
        total_memory_usage += size;
        total_asset_count += 1;
        
        if (cJSON_IsNumber(lru)) {
            if (lru->valuedouble < max_barrier) {
                _clear_state(state);
                cJSON_DetachItemViaPointer(store->state, state);
                cJSON_Delete(state);
                pruned_old_count += 1;
                pruned_old_size += size;
            }
        } else if (lru == NULL) {
            printf("missing lru\n");
        } else if (cJSON_IsBool(lru)) {
            static_asset_count += 1;
            static_memory_usage += size;
        }
        state = next;
    }
    
    // If we just have too many assets then prune some more
    if (total_asset_count > kCacheMaxCount || total_memory_usage > kCacheMaxBytes) { // todo: check memory usage and sort on impact as well.
        printf("assetstore: %d assets in cache using %zu bytes\n", total_asset_count, total_memory_usage);
        printf("assetstore: Limits are set to %d assets using %zu bytes\n", kCacheMaxCount, kCacheMaxBytes);
        int mode = total_memory_usage > kCacheMaxBytes ? 1 : 0;
        
        int count_to_evict = (mode == 0) ? total_asset_count - kCacheMaxCount : 0;
        size_t bytes_to_free = (mode == 1) ? total_memory_usage - kCacheMaxBytes : 0;
        
        if (mode == 0) {
            printf("assetstore: Trying to remove %d assets\n", count_to_evict);
        }
        if (mode == 1) {
            printf("assetstore: Trying to free up %zu bytes\n", bytes_to_free);
        }
        
        cJSON **candidates = malloc(total_asset_count * sizeof(cJSON*));
        cJSON **cursor = candidates;
        int candidates_count = 0;
        state = store->state->child;
        double min_barrier = time - kCacheMinAge; // don't evict items used too recently
        while(state) {
            cJSON *lru = cJSON_GetObjectItemCaseSensitive(state, "lru");
            // skips static (lru is a bool:true), and too recently used
            // too recently used: obviously in use right now, or at least is being downloaded
            if (cJSON_IsNumber(lru) && lru->valuedouble < min_barrier) {
                *cursor = state;
                ++cursor;
                ++candidates_count;
            }
            state = state->next;
        }
        if (mode == 1) {
            qsort(candidates, candidates_count, sizeof(cJSON*), _lru_compare_size);
        } else {
            qsort(candidates, candidates_count, sizeof(cJSON*), _lru_compare_age);
        }
        for(int i = 0; i < candidates_count; i++) {
            cJSON *state = candidates[i];
            size_t size = cJSON_GetObjectItemCaseSensitive(state, "total_size")->valueint;
            _clear_state(state);
            cJSON_DetachItemViaPointer(store->state, state);
            cJSON_Delete(state);
            
            if (mode == 0) {
                pruned_max_count += 1;
                pruned_max_size += size;
                
                if (--count_to_evict <= 0) {
                    break;
                }
            } else if (mode == 1) {
                pruned_memory_count += 1;
                pruned_memory_size += size;
                
                bytes_to_free -= size;
                if (bytes_to_free < kCacheMaxBytes) {
                    break;
                }
            }
        }
        free(candidates);
    }
    int pruned_total_count = pruned_old_count + pruned_max_count + pruned_memory_count;
    size_t pruned_total_size = pruned_old_size + pruned_max_size + pruned_memory_size;
    
    total_asset_count -= pruned_total_count;
    total_memory_usage -= pruned_total_size;
    
    if (pruned_total_count > 0) {
        printf("assetstore: Removed %d assets freeing %zu bytes.\n", pruned_total_count, pruned_total_size);
    }
    printf("assetstore: %d assets in cache using %zu bytes, whereof %d are static using %zu bytes.\n", total_asset_count, total_memory_usage, static_asset_count, static_memory_usage);
}

int _assetstore_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size) {
    
    mtx_lock(&store->lock);
    
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    if (!cJSON_IsObject(state)) {
        mtx_unlock(&store->lock);
        return -1;
    }
    
    _lru_tag(state);
    _lru_prune(store);
    
    mtx_unlock(&store->lock);
    
    return -1;
}

int _assetstore_write(assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size) {
    assert(store);
    assert(asset_id);
    assert(data);
    
    mtx_lock(&store->lock);
    // get or create the ranges from asset state in the store state
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *ranges = NULL;
    if (cJSON_IsObject(state)) {
        ranges = cJSON_GetObjectItem(state, "ranges");
        cJSON *file = cJSON_GetObjectItem(state, "data");
        
        // if there is a state then there is a "ranges" and "data"
        assert(ranges || (cJSON_IsTrue(cJSON_GetObjectItem(state, "complete"))));
        assert(cJSON_IsString(file));
    } else {
        state = cJSON_AddObjectToObject(store->state, asset_id);
        cJSON_AddBoolToObject(state, "complete", 0);
        cJSON_AddItemToObject(state, "total_size", cJSON_CreateNumber((double)total_size));
        ranges = cJSON_AddArrayToObject(state, "ranges");
    }
    
    _merge_range(ranges, offset, offset + length);
    _update_asset_state(store, asset_id);
    
    _lru_tag(state);
    _lru_prune(store);

    mtx_unlock(&store->lock);
    
    return length;
}


int _memstore_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size) {
    
    mtx_lock(&store->lock);
    
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    if (!cJSON_IsObject(state)) {
        mtx_unlock(&store->lock);
        return -1;
    }
    
    cJSON *size = cJSON_GetObjectItem(state, "total_size");
    if (!cJSON_IsNumber(size)) {
        mtx_unlock(&store->lock);
        return -2;
    }
    *out_total_size = (size_t)cJSON_GetNumberValue(size);
    
    cJSON *file = cJSON_GetObjectItem(state, "data");
    if (!cJSON_IsString(file)) {
        mtx_unlock(&store->lock);
        return -3;
    }
    uint8_t *file_data = strtol(file->valuestring, NULL, 10);
    length = min(length, *out_total_size - offset);
    memcpy(buffer, file_data + offset, length);
    
    _lru_tag(state);
    _lru_prune(store);
    
    mtx_unlock(&store->lock);
    
    return length;
}

int _memstorestore_write(assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size) {
    assert(store);
    assert(asset_id);
    assert(data);
    
    mtx_lock(&store->lock);
    // get or create the ranges from asset state in the store state
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    uint8_t *file_data = NULL;
    cJSON *ranges = NULL;
    if (cJSON_IsObject(state)) {
        ranges = cJSON_GetObjectItem(state, "ranges");
        cJSON *file = cJSON_GetObjectItem(state, "data");
        
        // if there is a state then there is a "ranges" and "data"
        assert(ranges || (cJSON_IsTrue(cJSON_GetObjectItem(state, "complete"))));
        assert(cJSON_IsString(file));
        
        file_data = strtol(file->valuestring, NULL, 10);
        assert(file_data);
    } else {
        state = cJSON_AddObjectToObject(store->state, asset_id);
        file_data = malloc(total_size);
        if (file_data == 0) {
            log("assetstore: Failed to allocate %ld bytes storage for asset %s", total_size, asset_id);
            mtx_unlock(&store->lock);
            return -1;
        }
        cJSON_AddBoolToObject(state, "complete", 0);
        cJSON_AddItemToObject(state, "total_size", cJSON_CreateNumber((double)total_size));
        ranges = cJSON_AddArrayToObject(state, "ranges");
        
        // warning: hacky hack-hack
        char num[255];
        sprintf(num, "%ld", (long)file_data);
        cJSON_AddStringToObject(state, "data", num);
    }
    assert(file_data);
    assert(offset + length <= total_size);
    memcpy(file_data + offset, data, length);
    
    _merge_range(ranges, offset, offset + length);
    _update_asset_state(store, asset_id);
    
    _lru_tag(state);
    _lru_prune(store);

    mtx_unlock(&store->lock);
    
    return length;
}


void _clear_state_asset(assetstore *store, const char *asset_id) {
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    if (cJSON_IsObject(state)) {
        _clear_state(state);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(store->state, asset_id);
}


int asset_memstore_init(assetstore *store) {
    assetstore_init(store);
    
    asset_memstore *memstore = calloc(1, sizeof(asset_memstore));
    
    memstore->_interface = store;
    store->_impl = (void*)memstore;
    
    store->use_cache = 1;
    store->get_missing_ranges = _memstore_get_missing_ranges;
    store->get_state = _memstore_state;
    store->read = _memstore_read;
    store->write = _memstorestore_write;
    return 0;
}

void asset_memstore_deinit(assetstore *_store) {
    cJSON *state = _store->state->child;
    while (state) {
        _clear_state(state);
        state = state->next;
    }
    assetstore_deinit(_store);
}

int assetstore_init(assetstore *store) {
    mtx_init(&store->lock, mtx_plain);
    store->_impl = NULL;
    store->state = cJSON_CreateObject();
    store->get_missing_ranges = _memstore_get_missing_ranges;
    store->get_state = _memstore_state;
    store->read = _assetstore_read;
    store->write = _assetstore_write;
    store->next_state_prune = 0;
    store->use_cache = 0;
    store->cache_count = 0;
    store->cache_size = 0;
    return 0;
}

void assetstore_deinit(assetstore *store) {
    cJSON_Delete(store->state);
    store->state = NULL;
    store->get_missing_ranges = NULL;
    store->get_state = NULL;
    store->read = NULL;
    store->write = NULL;
    mtx_destroy(&store->lock);
}


// ---- Public API

int assetstore_read(struct assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size) {
    return store->read(store, asset_id, offset, buffer, length, out_total_size);
}

int assetstore_write(struct assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size) {
    return store->write(store, asset_id, offset, data, length, total_size);
}

int assetstore_get_is_asset_complete(struct assetstore *store, const char *asset_id) {
    int complete = 0;
    if (assetstore_get_state(store, asset_id, NULL, &complete, NULL, NULL) == 0) {
        return complete;
    }
    return 0;
}

int assetstore_get_state(struct assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count, size_t *out_total_size) {
    return store->get_state(store, asset_id, out_exists, out_complete, out_regions_count, out_total_size);
}

size_t assetstore_get_missing_ranges(struct assetstore *store, const char *asset_id, size_t out_ranges[], size_t count) {
    return store->get_missing_ranges(store, asset_id, out_ranges, count);
}

char *asset_generate_identifier(const uint8_t *bytes, size_t size);
int asset_memstore_register_asset_nocopy(struct assetstore *store, const char *asset_id, const uint8_t *data, size_t length) {
    
    mtx_lock(&store->lock);
    
    char *id = asset_generate_identifier(data, length);
    _clear_state_asset(store, id);
    cJSON *state = cJSON_AddObjectToObject(store->state, id);
    cJSON_AddBoolToObject(state, "complete", 1);
    // warning: hacky hack-hack
    char num[255];
    sprintf(num, "%ld", (long)data);
    cJSON_AddStringToObject(state, "data", num);
    cJSON_AddNumberToObject(state, "total_size", length);
    cJSON_AddBoolToObject(state, "lru", true);
    
    mtx_unlock(&store->lock);
    
    log("assetstore: Did register complete asset '%s': %s\n", id, cJSON_Print(state));
    
    return 0;
}

uint8_t *asset_memstore_get_data_pointer(assetstore *store, const char *asset_id) {
    mtx_lock(&store->lock);
    
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    if (!cJSON_IsObject(state)) {
        mtx_unlock(&store->lock);
        return 0;
    }
    
    cJSON *complete = cJSON_GetObjectItemCaseSensitive(state, "complete");
    if (!cJSON_IsBool(complete) || cJSON_IsFalse(complete)) {
        mtx_unlock(&store->lock);
        return 0;
    }
    
    cJSON *file = cJSON_GetObjectItem(state, "data");
    if (!cJSON_IsString(file)) {
        mtx_unlock(&store->lock);
        return 0;
    }
    uint8_t *file_data = strtol(file->valuestring, NULL, 10);
    
    mtx_unlock(&store->lock);
    
    return file_data;
}
