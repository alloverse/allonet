
#include <unity.h>
#include <allonet/jobs.h>
#include <allonet/asset.h>
#include <stdlib.h>
#include <allonet/arr.h>
#include <stdbool.h>
#include <memory.h>

static scheduler jobs;

bool did_send_data;
bool did_recv_data;

void send_data(char *b, int c) {
    
}

int request_data(char *asset_id, char *b, int offset, int len) {
    strcpy(b, asset_id);
    return strlen(asset_id);
}


void test_(void) {
    
    char buffer[100];
    
    asset_job job;
    job.asset_id = "Hello world!";
    job.buffer = buffer;
    job.buffer_size = 100;
    job.network_max_accept_bytes = 40;
    job.offset = 0;
    job.request_data = request_data;
    job.send_data = send_data;
    
    scheduler_add(&jobs, &job, asset_send_work);
    
    scheduler_tick(&jobs);
    
    
}

void setUp() {
    did_recv_data = false;
    did_send_data = false;
    scheduler_init(&jobs);
}

void tearDown() {
    scheduler_remove_all(&jobs);
}


int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_);

    return UNITY_END();
}
