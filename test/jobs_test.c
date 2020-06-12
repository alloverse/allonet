#include <unity.h>
#include <allonet/jobs.h>
#include <stdlib.h>
#include <allonet/arr.h>
#include <stdbool.h>

static scheduler jobs;

bool work1_int(void *data) {
    int *i = (int*)data;
    (*i)++;
    return (*i) != 2;
}

void test_tick(void) {
    int value1 = 0;
    int value2 = 0;
    
    scheduler_add(&jobs, &value1, work1_int);
    scheduler_add(&jobs, &value2, work1_int);
    TEST_ASSERT_EQUAL_INT(jobs.jobs.length, 2);
    
    scheduler_tick(&jobs);
    TEST_ASSERT_EQUAL_INT(jobs.jobs.length, 2);
    TEST_ASSERT_EQUAL_INT(value1, 1);
    TEST_ASSERT_EQUAL_INT(value2, 1);
    
    scheduler_tick(&jobs);
    TEST_ASSERT_EQUAL_INT(jobs.jobs.length, 0);
    TEST_ASSERT_EQUAL_INT(value1, 2);
    TEST_ASSERT_EQUAL_INT(value2, 2);
}

void test_remove_all() {
    int value = 0;
    
    scheduler_add(&jobs, &value, work1_int);
    scheduler_add(&jobs, &value, work1_int);
    TEST_ASSERT_EQUAL_INT(jobs.jobs.length, 2);
    
    scheduler_remove_all(&jobs);
    TEST_ASSERT_EQUAL_INT(jobs.jobs.length, 0);
    
    scheduler_tick(&jobs);
    TEST_ASSERT_EQUAL_INT(value, 0);
    TEST_ASSERT_EQUAL_INT(value, 0);
}


void setUp() {
    scheduler_init(&jobs);
}

void tearDown() {
    scheduler_remove_all(&jobs);
}


int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tick);
    RUN_TEST(test_remove_all);

    return UNITY_END();
}
