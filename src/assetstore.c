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

#define min(a,b) a < b ? a : b
#define max(a,b) a > b ? a : b
#define log(f_, ...) printf((f_), ##__VA_ARGS__)


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
    int low = 0;
    int count = 0;
    int i = 0;
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

void _update_asset_state(assetstore *store, const char *asset_id) {
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *ranges = cJSON_GetObjectItem(state, "ranges");
    
    // if we have exactly 1 range that is 0...total_size:
    // TODO: confirm that the file sha is equal to the asset_id
    // then we have the complete file
    cJSON *total_size = cJSON_GetObjectItem(state, "total_size");
    if (cJSON_GetArraySize(ranges) == 1) {
        cJSON *range = _range(ranges, 0);
        if (_first(range) == 0 && _last(range) == total_size->valueint) {
            cJSON_ReplaceItemInObjectCaseSensitive(state, "complete", cJSON_CreateBool(1));
            cJSON_DeleteItemFromObject(state, "ranges");
            log("assetstore: Asset %s is complete\n", asset_id);
        }
    }
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

    mtx_unlock(&store->lock);
    
    return length;
}

void _clear_state(cJSON *state) {
    if (cJSON_IsObject(state)) {
        cJSON *file = cJSON_GetObjectItem(state, "data");
        if (cJSON_IsRaw(file)) {
            free(file->valuestring);
        }
        if (cJSON_IsString(file)) {
            uint8_t *data = (uint8_t *)strtol(file->valuestring, NULL, 10);
            free(data);
        }
    }
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
    
    memstore->interface = store;
    store->_impl = (void*)memstore;
    
    store->get_missing_ranges = _memstore_get_missing_ranges;
    store->get_state = _memstore_state;
    store->read = _memstore_read;
    store->write = _memstorestore_write;
    return 0;
}

void asset_memstore_deinit(assetstore *_store) {
    asset_memstore *store = (asset_memstore*)_store->_impl;
    
    cJSON *state = _store->state->child;
    while (state) {
        _clear_state(state);
        state = state->next;
    }
    assetstore_deinit(_store);
}

int assetstore_init(assetstore *store) {
    store->_impl = NULL;
    store->state = cJSON_CreateObject();
    store->get_missing_ranges = _memstore_get_missing_ranges;
    store->get_state = _memstore_state;
    store->read = NULL;
    store->write = NULL;
}

void assetstore_deinit(assetstore *store) {
    cJSON_Delete(store->state);
    store->state = NULL;
    store->get_missing_ranges = NULL;
    store->get_state = NULL;
    store->read = NULL;
    store->write = NULL;
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

int asset_memstore_register_asset_nocopy(struct assetstore *store, const char *asset_id, const uint8_t *data, size_t length) {
    
    mtx_lock(&store->lock);
    
    _clear_state_asset(store, asset_id);
    cJSON *state = cJSON_AddObjectToObject(store->state, asset_id);
    cJSON_AddBoolToObject(state, "complete", 1);
    cJSON_AddRawToObject(state, "data", (const char *)data);
    cJSON_AddNumberToObject(state, "total_size", length);
    
    mtx_unlock(&store->lock);
    
    log("assetstore: Did register complete asset %s\n", cJSON_Print(state));
    
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
