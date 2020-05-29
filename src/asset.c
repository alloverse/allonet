
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "inlinesys/queue.h"

typedef struct allo_asset_job {
    int mode; // 0 = nothing; 1 = send; 2 = recv;
    // mode 1: fill data with up to len bytes. return number of bytes written
    // mode 2: read len bytes from data. return number of bytes read
    int(*data_callback)(struct allo_asset_job *job, char *data, int len);

    // When job is completed
    void(*done_callback)(struct allo_asset_job *job);

    // When job fails (job will be terminated)
    void(*error_callback)(struct allo_asset_job *job, char *error_message);

    // User data
    void *user;

    // Total bytes to process
    int count;

    // Bytes completed
    int completed;
    
    // used by allonet
    void *_allo;

    struct allo_asset_job *_next;
} allo_asset_job;

typedef struct {
    // Root job. Set common callbacks;
    allo_asset_job job;

    allo_asset_job *_last_job;

} allo_asset_t;


void allo_asset_job_add(allo_asset_t *asset, allo_asset_job *job) {
    if (asset->_last_job == 0) {
        asset->_last_job = job;
    } else {
        asset->_last_job->_next = job;
        asset->_last_job = job;
    }
    job->_next = 0;
}

void allo_asset_job_remove(allo_asset_t *asset, allo_asset_job *job) {
    allo_asset_job *prev = asset->job._next;
    while(prev != 0 && prev->_next != job) {
        prev = prev->_next;
    }
    if (prev != 0) {
        prev->_next = job->_next;
    }
    free(job);
}

allo_asset_job *allo_asset_send(int size) {
    allo_asset_job *job = calloc(1, sizeof(allo_asset_job));
    memset(job, 0, sizeof(allo_asset_job));
}

void _allo_asset_pump_send(allo_asset_t *asset, allo_asset_job *job) {

}

void _allo_asset_pump_recv(allo_asset_t *asset, allo_asset_job *job) {
    
}

int allo_asset_pump(allo_asset_t *asset) {
    allo_asset_job *job = asset->job._next;

    while (job) {
        if (job->mode == 0) {
            // nothing
        } else if (job->mode == 1) {
            _allo_asset_pump_send(asset, job);
        } else if (job->mode == 2) {
            _allo_asset_pump_recv(asset, job);
        } else {
            if(job->error_callback) job->error_callback(job, "invalid mode");
            allo_asset_job_remove(asset, job);
        }
        job = job->_next;
    }

    return 0;
}



#include <stdio.h>

int main() {
    allo_asset_job *jobs;

    while (allo_asset_pump(jobs)) {

    }
}