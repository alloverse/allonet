
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

static assetstore *store;

void test_(void) {

    TEST_ASSERT_EQUAL_INT(12, assetstore_write(store, "1", 0, (uint8_t*)"Hello World", 12));
    char buff[100];
    TEST_ASSERT_EQUAL_INT(12, assetstore_read(store, "1", 0, (uint8_t*)buff, 12));
    TEST_ASSERT_EQUAL_STRING("Hello World", buff);
    
    TEST_ASSERT_EQUAL_INT(6, assetstore_read(store, "1", 6, (uint8_t*)buff, 6));
    TEST_ASSERT_EQUAL_STRING("World", buff);
}

void _merge_range(cJSON *ranges, size_t start, size_t end);

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

void _test_merge(cJSON *ranges, size_t start, size_t end, cJSON *expected) {
    _merge_range(ranges, start, end);
    TEST_ASSERT_EQUAL_STRING(cJSON_PrintUnformatted(expected), cJSON_PrintUnformatted(ranges));
}

void test_merge_range() {

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

void setUp() {
    store = assetstore_open("asset_test_cache");
}

void tearDown() {
    assetstore_close(store);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_);
    RUN_TEST(test_merge_range);

    return UNITY_END();
}
