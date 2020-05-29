
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

// A super simple "scheduler" for recurring tasks
typedef struct allo_job {
    // Called when you should do some work
    // returning !0 will result in job termination
    int(*work)(struct allo_job *job);

    // state for worker
    void *data;    

    struct allo_job *_next;
} allo_job;

allo_job *allo_job_add(allo_job *after, int(*work)(allo_job *)) {
    allo_job *job = (allo_job *)malloc(sizeof(allo_job)); 
    memset(job, 0, sizeof(allo_job));
    if (after) {
        job->_next = after->_next;
        after->_next = job;
    }
    job->work = work;
    return job;
}

// Process `job` and all job after `job`
int allo_job_pump(allo_job *job) {
    while(job != 0) {
        // If job completes
        if (job->work(job)) {
            // get next and free
            allo_job *completed = job;
            job = completed->_next;
            free(completed);
        } else {
            job = job->_next;
        }
    }
    return 0;
}


int when_worker_complete(allo_job *job) {
    return 1;
}

int worker(allo_job *job) {
    int s = (int)job->data;

    switch (s) {
    case 0:
        job->data = (void*)1;
        break;
    case 1:
        job->data = (void*)2;
        break;
    case 2:
        allo_job_add(job, when_worker_complete);
        return 1;
    default:
        break;
    }

    return 0;
}

int main() {
    // allo_job *job = allo_job_add(0, worker);
    // allo_job_pump(job);
    return 0;
}