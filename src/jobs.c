//
//  jobs.c
//  allocubeappliance
//
//  Created by Patrik on 2020-06-12.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memmove for arr_splice
#include <allonet/jobs.h>

struct scheduler;

void scheduler_init(scheduler *s) {
    arr_init(&s->jobs);
}

void scheduler_add(scheduler *s, void *data, bool(*work)(void *)) {
    job job;
    job.work = work;
    job.data = data;
    arr_push(&s->jobs, job);
}

void scheduler_remove_all(scheduler *s) {
    arr_clear(&s->jobs);
}

// Stop the job. You should free the data first.
void scheduler_stop(scheduler *s, job *job) {
    for (int i = 0; i < s->jobs.length; i++) {
        struct job *j = &s->jobs.data[i];
        if (j->data == job->data && j->work == job->work) {
            arr_splice(&s->jobs, i, 1);
            return;
        }
    }
}

void scheduler_tick(scheduler *s) {
    int i = 0;
    while (i < s->jobs.length) {
        struct job *job = &s->jobs.data[i];
        // if work returns false we remove the job instead of incrementing i
        if (job->work(job->data)) {
            ++i;
        } else {
            arr_splice(&s->jobs, i, 1);
        }
    }
}
