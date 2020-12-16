//
//  asset_store.c
//  allonet
//
//  Created by Patrik on 2020-11-27.
//
#define ASS_FTW 0
#define _XOPEN_SOURCE 600
#ifdef _WIN32
 #include <direct.h>
 #include <crtdefs.h>
 #define getcwd _getcwd // stupid MSFT "deprecation" warning
 #ifndef PATH_MAX
  #include <windows.h>
  #define PATH_MAX MAX_PATH
 #endif
#else
 #include <unistd.h>
#endif
#include <allonet/arr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#if ASS_FTW
 #include <ftw.h>
#endif
#include "assetstore.h"
#include "os.h"

#define min(a,b) a < b ? a : b
#define max(a,b) a > b ? a : b
#define log(f_, ...) printf((f_), ##__VA_ARGS__)

void rek_mkdir(char *path) {
    char *sep = strrchr(path, '/');
    if(sep != NULL) {
        *sep = 0;
        rek_mkdir(path);
        *sep = '/';
    }
    if(mkdir(path, 0777) && errno != EEXIST) {
        printf("error while trying to create '%s'\n%m\n", path);
    }
}

FILE *fopen_mkdir(char *path, char *mode) {
    char *sep = strrchr(path, '/');
    if(sep) {
        char *path0 = strdup(path);
        path0[ sep - path ] = 0;
        rek_mkdir(path0);
        free(path0);
    }
    return fopen(path,mode);
}

size_t _first(cJSON *range) {
    return (size_t)cJSON_GetArrayItem(range, 0)->valueint;
}

size_t _last(cJSON *range) {
    return (size_t)cJSON_GetArrayItem(range, 1)->valueint;
}

cJSON *_range(cJSON *ranges, int index) {
    return cJSON_GetArrayItem(ranges, index);
}

////////////////////////////////////////////////////////////////////////////////
// Return the input path in a canonical form. This is achieved by expanding all
// symbolic links, resolving references to "." and "..", and removing duplicate
// "/" characters.
//
// If the file exists, its path is canonicalized and returned. If the file,
// or parts of the containing directory, do not exist, path components are
// removed from the end until an existing path is found. The remainder of the
// path is then appended to the canonical form of the existing path,
// and returned. Consequently, the returned path may not exist. The portion
// of the path which exists, however, is represented in canonical form.
//
// If successful, this function returns a C-string, which needs to be freed by
// the caller using free().
//
// ARGUMENTS:
//   file_path
//   File path, whose canonical form to return.
//
// RETURNS:
//   On success, returns the canonical path to the file, which needs to be freed
//   by the caller.
//
//   On failure, returns NULL.
////////////////////////////////////////////////////////////////////////////////
char *make_file_name_canonical(char const *file_path)
{
    char *canonical_file_path  = NULL;
    unsigned int file_path_len = strlen(file_path);
    
    if (file_path_len > 0)
    {
        canonical_file_path = realpath(file_path, NULL);
        if (canonical_file_path == NULL && errno == ENOENT)
        {
            // The file was not found. Back up to a segment which exists,
            // and append the remainder of the path to it.
            char *file_path_copy = NULL;
            if (file_path[0] == '/'                ||
                (strncmp(file_path, "./", 2) == 0) ||
                (strncmp(file_path, "../", 3) == 0))
            {
                // Absolute path, or path starts with "./" or "../"
                file_path_copy = strdup(file_path);
            }
            else
            {
                // Relative path
                file_path_copy = (char*)malloc(strlen(file_path) + 3);
                strcpy(file_path_copy, "./");
                strcat(file_path_copy, file_path);
            }
            
            // Remove path components from the end, until an existing path is found
            for (int char_idx = strlen(file_path_copy) - 1;
                 char_idx >= 0 && canonical_file_path == NULL;
                 --char_idx)
            {
                if (file_path_copy[char_idx] == '/')
                {
                    // Remove the slash character
                    file_path_copy[char_idx] = '\0';
                    
                    canonical_file_path = realpath(file_path_copy, NULL);
                    if (canonical_file_path != NULL)
                    {
                        // An existing path was found. Append the remainder of the path
                        // to a canonical form of the existing path.
                        char *combined_file_path = (char*)malloc(strlen(canonical_file_path) + strlen(file_path_copy + char_idx + 1) + 2);
                        strcpy(combined_file_path, canonical_file_path);
                        strcat(combined_file_path, "/");
                        strcat(combined_file_path, file_path_copy + char_idx + 1);
                        free(canonical_file_path);
                        canonical_file_path = combined_file_path;
                    }
                    else
                    {
                        // The path segment does not exist. Replace the slash character
                        // and keep trying by removing the previous path component.
                        file_path_copy[char_idx] = '/';
                    }
                }
            }
            
            free(file_path_copy);
        }
    }
    
    return canonical_file_path;
}

char *really_realpath(const char *path) {
    char *maybe = make_file_name_canonical(path);
    if (maybe == NULL || strlen(maybe) == 0) {
        char workdir[PATH_MAX];
        allo_os_working_dir(workdir, PATH_MAX);
        snprintf(workdir, PATH_MAX, "%s/%s", workdir, path);
        maybe = make_file_name_canonical(workdir);
    }
    return maybe;
}

char __asset_path[PATH_MAX];
char *_asset_path(assetstore *store, const char *asset_id) {
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *path = cJSON_GetObjectItem(state, "path");
    if (cJSON_IsString(path)) {
        return cJSON_GetStringValue(path);
    }
    sprintf(__asset_path, "%s/%s", store->disk_path, asset_id);
    return __asset_path;
}



char *assetstore_asset_path(assetstore *store, const char *asset_id) {
    mtx_lock(&store->lock);
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *complete = cJSON_GetObjectItem(state, "complete");
    // check that it exists and is complete
    if (!cJSON_IsObject(state) || !cJSON_IsTrue(complete)) {
        log("assetstore: Asset %s does not exist or is incomplete\n", asset_id);
        mtx_unlock(&store->lock);
        return NULL;
    }
    
    char *value = NULL;
    // check if it's in cache or on path
    cJSON *path = cJSON_GetObjectItem(state, "path");
    if (cJSON_IsString(path)) {
        value = strdup(cJSON_GetStringValue(path));
    } else {
        size_t len = strlen(store->disk_path) + strlen(asset_id) + 2;
        value = malloc(len);
        snprintf(value, len, "%s/%s", store->disk_path, asset_id);
    }
    
    mtx_unlock(&store->lock);
    return value;
}

int __disk_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size) {
    assert(asset_id);
    assert(buffer);
    assert(out_total_size);
    
    char *fpath = _asset_path(store, asset_id);
    
    struct stat st;
    if (stat(fpath, &st) == 0) {
        *out_total_size = st.st_size;
    }
    
    int f = open(fpath, O_RDONLY);
    if (f <= 0) {
        log("assetstore: Could not open %s for reading: %s\n", fpath, strerror(errno));
        return -1;
    }
    
    int64_t rlen = pread(f, buffer, length, offset);
    close(f);
    if (rlen < 0) {
        log("assetstore: Could only read %lld of %ld bytes of %s. %s\n", rlen, length, asset_id, strerror(errno));
    }
    return (int)rlen;
}

int __disk_write(assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size) {
    char *fpath = _asset_path(store, asset_id);
    int res = 0;
    int f = open(fpath, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (f ==  0) {
        log("assetstore: Failed to open file for writing: %s", fpath);
        return 0;
    }
    res = pwrite(f, data, length, offset);
    close(f);
    if (res != length) {
        log("assetstore: Could only write %d of %ld bytes of %s\n", res, length, asset_id);
    }
    return res;
}

static mtx_t _global_lock;
arr_t(assetstore *) _global_stores = {0xdead};
void _assetstore_close(assetstore *store);

void assetstore_init() {
    mtx_init(&_global_lock, mtx_plain);
    arr_init(&_global_stores);
}

void assetstore_deinit() {
    for (int i = 0; i < _global_stores.length; i++) {
        _assetstore_close(_global_stores.data[i]);
    }
    
    arr_free(&_global_stores);
    mtx_destroy(&_global_lock);
}

assetstore *assetstore_open(const char *disk_path) {
    if (_global_stores.data == (assetstore**)0xdead) {
        log("assetstore: Not initialized yet\n");
        exit(555);
    }
    assetstore *store = NULL;
    
    char *absolute_disk_path = really_realpath(disk_path);
    
    
    mtx_lock(&_global_lock);
    for (int i = 0; i < _global_stores.length; i++) {
        if (strcmp(_global_stores.data[i]->disk_path, absolute_disk_path) == 0) {
            store = _global_stores.data[i];
            ++store->refcount;
            mtx_unlock(&_global_lock);
            return store;
        }
    }
    mtx_unlock(&_global_lock);
    
    store = malloc(sizeof(assetstore));
    mtx_init(&store->lock, mtx_plain);
    store->refcount = 1;
    store->disk_path = strdup(absolute_disk_path);
    store->read = __disk_read;
    store->write = __disk_write;
    
    mtx_lock(&_global_lock);
    arr_push(&_global_stores, store);
    mtx_unlock(&_global_lock);
    
    size_t len = strlen(disk_path) + 12;
    char *statefile = malloc(len);
    snprintf(statefile, len, "%s/%s", disk_path, "state.json");
    FILE *f = fopen(statefile, "r");
    free(statefile);
    if (f) {
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            log("assetstore: Unable to seek state file\n");
            return store;
        }
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if(size == 0) {
            log("assetstore: statefile is empty\n");
            store->state = cJSON_CreateObject();
            fclose(f);
            return store;
        }
        char *contents = (char *)malloc(size);
        if(contents == NULL) {
            log("assetstore: could not fit statefile in memory\n");
            store->state = cJSON_CreateObject();
            fclose(f);
            return store;
        }
        if (fread(contents, 1, size, f) != size) {
            log("assetstore: Failed to read the whole statefile\n");
            store->state = cJSON_CreateObject();
            free(contents);
            fclose(f);
            return store;
        }
        
        fclose(f);
        store->state = cJSON_Parse(contents);
        
        if (store->state == NULL) {
            log("assetstore: Failed to parse statefile\n");
            store->state = cJSON_CreateObject();
            free(contents);
            return store;
        }
        
        free(contents);
    } else {
        log("assetstore: Initializing store at %s\n", disk_path);
        f = fopen_mkdir(statefile, "w");
        if (f) fclose(f);
        store->state = cJSON_CreateObject();
    }
    
    return store;
}

void _assetstore_close(assetstore *store) {
    free((void*)store->disk_path);
    cJSON_Delete(store->state);
    store->disk_path = NULL;
    store->state = NULL;
    free(store);
}

void assetstore_close(assetstore *store) {
    mtx_lock(&_global_lock);
    if (--store->refcount > 0) {
        mtx_unlock(&_global_lock);
        return;
    }
    
    for (int i = 0; i < _global_stores.length; i++) {
        if (_global_stores.data[i] == store) {
            arr_splice(&_global_stores, i, 1);
            break;
        }
    }
    
    _assetstore_close(store);
    
    mtx_unlock(&_global_lock);
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

int assetstore_state(assetstore *store, const char *asset_id, int *out_exists, int *out_complete, size_t *out_regions_count) {
    assert(store);
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
        *out_exists = 1;
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

int assetstore_asset_is_complete(assetstore *store, const char *asset_id) {
    assert(asset_id);
    
    mtx_lock(&store->lock);
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *complete = cJSON_GetObjectItem(state, "complete");
    int result = cJSON_IsTrue(complete);
    mtx_unlock(&store->lock);
    
    return result;
}

size_t assetstore_get_missing_ranges(assetstore *store, const char *asset_id, size_t *out_ranges, size_t count) {
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

int assetstore_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length, size_t *out_total_size) {
    assert(asset_id);
    assert(buffer);
    assert(out_total_size);
    return store->read(store, asset_id, offset, buffer, length, out_total_size);
}

void _write_state(assetstore *store) {
    size_t len = strlen(store->disk_path) + 12;
    char *statefile = malloc(len);
    snprintf(statefile, len, "%s/%s", store->disk_path, "state.json");
    FILE *f = fopen(statefile, "w");
    free(statefile);
    char *data = cJSON_Print(store->state);
    int res = 0;
    res = fwrite(data, strlen(data), 1, f);
    free(data),
    fclose(f);
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
            cJSON_ReplaceItemInObject(state, "complete", cJSON_CreateBool(1));
            cJSON_DeleteItemFromObject(state, "ranges");
            log("assetstore: Asset %s is complete\n", asset_id);
        }
    }
    
    _write_state(store);
}

int assetstore_write(assetstore *store, const char *asset_id, size_t offset, const uint8_t *data, size_t length, size_t total_size) {
    assert(store);
    assert(asset_id);
    assert(data);
    
    int bytes_written = store->write(store, asset_id, offset, data, length, total_size);
    
    mtx_lock(&store->lock);
    // get or create the ranges from asset state in the store state
    cJSON *state = cJSON_GetObjectItem(store->state, asset_id);
    cJSON *ranges = NULL;
    if (state) {
        ranges = cJSON_GetObjectItem(state, "ranges");
        // if there is a state then there is a "ranges"
        assert(ranges || (cJSON_IsTrue(cJSON_GetObjectItem(state, "complete"))));
    } else {
        state = cJSON_AddObjectToObject(store->state, asset_id);
        cJSON_AddBoolToObject(state, "complete", 0);
        cJSON_AddItemToObject(state, "total_size", cJSON_CreateNumber((double)total_size));
        ranges = cJSON_AddArrayToObject(state, "ranges");
    }
    
    if (bytes_written > 0) {
        _merge_range(ranges, offset, offset + bytes_written);
        _update_asset_state(store, asset_id);
    }
    mtx_unlock(&store->lock);
    
    return bytes_written;
}


// Temp hashing func
#include "sha1.h"


int _assimilate_file(const char *path, cJSON *store_state) {
    static const size_t buffsize = 1024*1024*10;
    uint8_t *buffer = malloc(buffsize);
    
    FILE *f = fopen(path, "rb");
    size_t rd;
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    while(1) {
        rd = fread(buffer, 1, buffsize, f);
        if(rd == 0) {
            break;
        }
        SHA1Update(&ctx, buffer, rd);
    }
    free(buffer);
    fclose(f);
    
    unsigned char sha[20] = {0};
    SHA1Final(sha, &ctx);
    
    char *xsha = malloc(21);
    snprintf(xsha, 21, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", sha[0], sha[1], sha[2], sha[3], sha[4], sha[5], sha[6], sha[7], sha[8], sha[9], sha[10], sha[11], sha[12], sha[13], sha[14], sha[15], sha[16], sha[17], sha[18], sha[19]);
    
    cJSON_DeleteItemFromObject(store_state, xsha);
    cJSON *state = cJSON_AddObjectToObject(store_state, xsha);
    free(xsha);
    cJSON_AddBoolToObject(state, "complete", 1);
    cJSON_AddStringToObject(state, "path", path);
    
    log("assetstore: Assimilated %s\n", cJSON_Print(state));
    return 0;
}

#if ASS_FTW
__thread cJSON *ass_state = 0;
//int _assimilate_ftw(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
//    return _assimilate_file(path);
//}
#endif
int assetstore_assimilate(assetstore *store, const char *path) {
    char *full_path = really_realpath(path);
    
    mtx_lock(&store->lock);
    

    //TODO: shold add whole dirs but cross platform iterating dirs is an epic in itself ofc.
#if ASS_FTW
    assert(ass_state == NULL);
    ass_state = store->state;
    nftw(full_path, _assimilate_ftw, 64, FTW_DEPTH | FTW_PHYS);
    ass_state = NULL;
#else
    _assimilate_file(full_path, store->state);
#endif
    
    _write_state(store);
    mtx_unlock(&store->lock);
    
    return 0;
}
