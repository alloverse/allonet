
#include <cJSON/cJSON.h>
#include <unity.h>
#include <allonet/jobs.h>
#include <allonet/asset.h>
#include <allonet/assetstore.h>
#include <stdlib.h>
#include <allonet/arr.h>
#include <stdbool.h>
#include <memory.h>
#include <stdarg.h>
#include <ftw.h>
#include <assert.h>

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a < _b ? _a : _b; })

static assetstore *store;

static const char *relative_path = "asset_test_cache";

#define TEST_DISK_SPACE 100
static uint8_t __test_disk[TEST_DISK_SPACE];
static size_t __test_disk_eof = TEST_DISK_SPACE;

int __test_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length) {
    assert(buffer);
    assert((offset + length) < TEST_DISK_SPACE);
    memcpy(buffer, &__test_disk[offset], length);
    return length;
}

int __test_write(assetstore *store, const char *asset_id, size_t offset, const u_int8_t *data, size_t length, size_t total_size) {
    assert(data);
    assert((offset + length) < TEST_DISK_SPACE);
    memcpy(&__test_disk[offset], data, length);
    return length;
}


void _resetTestDiskSpace() {
    assert(TEST_DISK_SPACE < 256);
    for (uint8_t i = 0; i < TEST_DISK_SPACE; i++) {
        __test_disk[i] = i;
    }
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);
    if (rv) perror(fpath);
    return rv;
}
int rmrf(const char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

void _reset_disk() {
    assetstore_close(store);
    assert(rmrf(relative_path) == 0);
    
    store = assetstore_open(relative_path);
}

void setUp() {
    _resetTestDiskSpace();
    store = assetstore_open(relative_path);
    store->read = __test_read;
    store->write = __test_write;
}

void tearDown() {
    assetstore_close(store);
    assert(rmrf(relative_path) == 0);
}


///Private assetstore.c declares
char *_asset_path(assetstore *store, const char *id);
int __disk_read(assetstore *store, const char *asset_id, size_t offset, uint8_t *buffer, size_t length);
int __disk_write(assetstore *store, const char *asset_id, size_t offset, const u_int8_t *data, size_t length, size_t total_size);

void test_assetstore_basic_disk_io() {
    char *path = _asset_path(store, "1");
    FILE *f;
    uint8_t buff[100];
    
    __disk_write(store, "1", 0, (uint8_t *)"Hello Alloverse!", 17, 17);
    f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    fread(buff, 1, 17, f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("Hello Alloverse!", buff);

    _reset_disk();
    
    // write the last 17/27 bytes
    __disk_write(store, "1", 10, (uint8_t *)"Hello Alloverse!", 17, 27);
    f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    // should find the 17 bytes on the correct offset
    fread(buff, 1, 27, f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("Hello Alloverse!", &buff[10]);
    
    free(path);
}

void test_assetstore_read_write() {

    int exists = 0, complete = 0;
    size_t region_count = 0;
    assetstore_state(store, "1", &exists, &complete, &region_count);
    TEST_ASSERT_EQUAL_INT(0, exists);
    TEST_ASSERT_EQUAL_INT(0, complete);
    TEST_ASSERT_EQUAL_INT(0, region_count);
    TEST_ASSERT_EQUAL_INT(12, assetstore_write(store, "1", 0, (uint8_t*)"Hello World", 12, 12));
    TEST_ASSERT_EQUAL_STRING("{\"1\":{\"complete\":true,\"total_size\":12,\"ranges\":[[0,12]]}}", cJSON_PrintUnformatted(store->state));
    
    assetstore_state(store, "1", &exists, &complete, &region_count);
    TEST_ASSERT_EQUAL_INT(1, exists);
    TEST_ASSERT_EQUAL_INT(1, complete);
    TEST_ASSERT_EQUAL_INT(0, region_count);
    
    size_t ranges[region_count*2];
    TEST_ASSERT_EQUAL_INT(0, assetstore_get_missing_ranges(store, "1", ranges, region_count));
    
    char buff[100];
    TEST_ASSERT_EQUAL_INT(12, assetstore_read(store, "1", 0, (uint8_t*)buff, 12));
    TEST_ASSERT_EQUAL_STRING("Hello World", buff);
    
    TEST_ASSERT_EQUAL_INT(6, assetstore_read(store, "1", 6, (uint8_t*)buff, 6));
    TEST_ASSERT_EQUAL_STRING("World", buff);
}

void test_assetstore_partial_read_write(void) {
    int exists = 0, complete = 0;
    size_t missing_regions_count = 0;
    assetstore_state(store, "1", &exists, &complete, &missing_regions_count);
    TEST_ASSERT_EQUAL_INT(0, exists);
    TEST_ASSERT_EQUAL_INT(0, complete);
    TEST_ASSERT_EQUAL_INT(0, missing_regions_count);
    TEST_ASSERT_EQUAL_INT(5, assetstore_write(store, "1", 0, (uint8_t*)"Hello", 5, 12));
    TEST_ASSERT_EQUAL_STRING("{\"1\":{\"complete\":false,\"total_size\":12,\"ranges\":[[0,5]]}}", cJSON_PrintUnformatted(store->state));
    
    assetstore_state(store, "1", &exists, &complete, &missing_regions_count);
    TEST_ASSERT_EQUAL_INT(1, exists);
    TEST_ASSERT_EQUAL_INT(0, complete);
    TEST_ASSERT_EQUAL_INT(1, missing_regions_count);
    
    size_t ranges[missing_regions_count*2];
    TEST_ASSERT_EQUAL_INT(1, assetstore_get_missing_ranges(store, "1", ranges, missing_regions_count));
    TEST_ASSERT_EQUAL_INT(6, ranges[0]);
    TEST_ASSERT_EQUAL_INT(6, ranges[1]);
    
    char buff[100];
    // reading the available bytes
    TEST_ASSERT_GREATER_OR_EQUAL(5, assetstore_read(store, "1", 0, (uint8_t*)buff, 12));
    TEST_ASSERT_EQUAL_CHAR_ARRAY("Hello\5\6\7", buff, 8); //567 is the default data from __test_disk, for verifying read and write position.
    
    // trying to read the unavailable bytes
    assetstore_state(store, "1", &exists, &complete, &missing_regions_count);
    TEST_ASSERT_EQUAL_INT(1, exists);
    TEST_ASSERT_EQUAL_INT(0, complete);
    TEST_ASSERT_EQUAL_INT(1, missing_regions_count);
    
    
    assetstore_write(store, "1", 6, (uint8_t*)"Allo", 5, 12);
    TEST_ASSERT_EQUAL_INT(6, assetstore_read(store, "1", 6, (uint8_t*)buff, 6));
    TEST_ASSERT_EQUAL_STRING("Allo", buff);
    
    TEST_ASSERT_EQUAL_INT(12, assetstore_read(store, "1", 0, (uint8_t*)buff, 12));
    TEST_ASSERT_EQUAL_STRING("Hello\5Allo", buff);
    
    assetstore_write(store, "1", 5, (uint8_t*)" ", 1, 12);
    TEST_ASSERT_EQUAL_INT(12, assetstore_read(store, "1", 0, (uint8_t*)buff, 12));
    TEST_ASSERT_EQUAL_STRING("Hello Allo", buff);
}

void test_assetstore_range_reads(void) {
    // Needs the _test_disk to contain sequential bytes
    uint8_t buff[20];
    
    assetstore_read(store, "1", 0, buff, 5);
    uint8_t e1[5] = { 0, 1, 2, 3, 4 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(e1, buff, 5);
    
    assetstore_read(store, "1", 10, buff, 5);
    uint8_t e2[5] = { 10, 11, 12, 13, 14 };
    TEST_ASSERT_EQUAL_CHAR_ARRAY(e2, buff, 5);
    
    assetstore_read(store, "1", 57, buff, 5);
    uint8_t e3[5] = { 57, 58, 59, 60, 61 };
    TEST_ASSERT_EQUAL_CHAR_ARRAY(e3, buff, 5);
}

cJSON *_ranges(size_t start, size_t end, ...) {
    cJSON *ranges = cJSON_CreateArray();
    int numbers[2] = {(int)start, (int)end};
    cJSON_AddItemToArray(ranges, cJSON_CreateIntArray(numbers, 2));
    va_list args;
    va_start(args, end);
    while(1) {
        numbers[0] = va_arg(args, int);
        if (numbers[0] == 0) break;
        numbers[1] = va_arg(args, int);
        cJSON_AddItemToArray(ranges, cJSON_CreateIntArray(numbers, 2));
    }
    va_end(args);
    return ranges;
}


size_t __missing_ranges_count(cJSON *ranges, size_t total_size);
void test_missing_ranges() {
    TEST_ASSERT_EQUAL_INT(4, __missing_ranges_count(_ranges(5, 6,  10, 15,  100, 200, NULL), 300));
}

void _merge_range(cJSON *ranges, size_t start, size_t end);
void _test_merge(cJSON *ranges, size_t start, size_t end, cJSON *expected) {
    _merge_range(ranges, start, end);
    TEST_ASSERT_EQUAL_STRING(cJSON_PrintUnformatted(expected), cJSON_PrintUnformatted(ranges));
}

void test_merge_range() {
    
    // ranges are inclusive
    _test_merge(cJSON_CreateArray(), 0, 0, _ranges(0, 0, NULL));
    _test_merge(_ranges(5, 7, NULL), 4, 4, _ranges(4, 7, NULL));
    _test_merge(_ranges(5, 7, NULL), 5, 5, _ranges(5, 7, NULL));
    _test_merge(_ranges(5, 7, NULL), 6, 6, _ranges(5, 7, NULL));
    _test_merge(_ranges(5, 7, NULL), 7, 7, _ranges(5, 7, NULL));
    _test_merge(_ranges(5, 7, NULL), 8, 8, _ranges(5, 8, NULL));

    // Insert before
    _test_merge(_ranges(5, 7, NULL), 1, 2, _ranges(1, 2, 5, 7, NULL));
    // Merge before
    _test_merge(_ranges(5, 7, NULL), 1, 6, _ranges(1, 7, NULL));
    // Merge inside
    _test_merge(_ranges(5, 7, NULL), 6, 6, _ranges(5, 7, NULL));
    // Merge around
    _test_merge(_ranges(5, 7, NULL), 1, 9, _ranges(1, 9, NULL));
    // Merge after
    _test_merge(_ranges(5, 7, NULL), 6, 8, _ranges(5, 8, NULL));
    // Insert after
    _test_merge(_ranges(5, 7, NULL), 9, 9, _ranges(5, 7, 9, 9, NULL));
    // Insert between
    _test_merge(_ranges(1, 3, 5, 7, NULL), 4, 4, _ranges(1, 7, NULL));
    // merge before between
    _test_merge(_ranges(1, 3, 5, 7, NULL), 3, 4, _ranges(1, 7, NULL));
    
    _test_merge(_ranges(1, 3, 5, 7, NULL), 4, 5, _ranges(1, 7, NULL));
    _test_merge(cJSON_CreateArray(), 1, 2, _ranges(1, 2, NULL));
    _test_merge(_ranges(1, 2, 100, 200, NULL), 40, 60, _ranges(1, 2, 40, 60, 100, 200, NULL));
    _test_merge(_ranges(1, 2, NULL), 2, 3, _ranges(1, 3, NULL));
    _test_merge(_ranges(1, 3, NULL), 6, 9, _ranges(1, 3, 6, 9, NULL));
    _test_merge(_ranges(1, 3, 6, 9, NULL), 2, 7, _ranges(1, 9, NULL));
    _test_merge(_ranges(5, 8, NULL), 1, 3, _ranges(1, 3, 5, 8, NULL));
    _test_merge(_ranges(5, 8, NULL), 10, 30, _ranges(5, 8, 10, 30, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 9, 11, _ranges(1, 2, 5, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 9, 10, _ranges(1, 2, 5, 10, 12, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 10, 11, _ranges(1, 2, 5, 8, 10, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 6, 8, 12, 15, 20, 21, NULL), 4, 4, _ranges(1, 2, 4, 4, 6, 8, 12, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 4, 4, _ranges(1, 2, 4, 8, 12, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 4, 18, _ranges(1, 2, 4, 18, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 10, 11, _ranges(1, 2, 5, 8, 10, 15, 20, 21, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 9, 30, _ranges(1, 2, 5, 30, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 1, 9999, _ranges(1, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 4, 22, _ranges(1, 2, 4, 22, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, NULL), 4, 20, _ranges(1, 2, 4, 21, NULL));
    
    
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 5, 9999, _ranges(1, 2, 5, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 6, 9999, _ranges(1, 2, 5, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 7, 9999, _ranges(1, 2, 5, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 8, 9999, _ranges(1, 2, 5, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 9, 9999, _ranges(1, 2, 5, 9999, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 10, 9999, _ranges(1, 2, 5, 8, 10, 9999, NULL));
    
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 1, 18, _ranges(1, 18, 20, 21, 40, 45, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 1, 19, _ranges(1, 21, 40, 45, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 1, 20, _ranges(1, 21, 40, 45, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 1, 21, _ranges(1, 21, 40, 45, NULL));
    _test_merge(_ranges(1, 2, 5, 8, 12, 15, 20, 21, 40, 45, NULL), 1, 22, _ranges(1, 22, 40, 45, NULL));
}


int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_assetstore_basic_disk_io);
    RUN_TEST(test_assetstore_range_reads);
    RUN_TEST(test_assetstore_read_write);
    RUN_TEST(test_merge_range);
    RUN_TEST(test_assetstore_partial_read_write);
    RUN_TEST(test_missing_ranges);

    return UNITY_END();
}
