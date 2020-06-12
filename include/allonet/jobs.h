//
//  jobs.h
//  allonet
//
//  Created by Patrik on 2020-06-12.
//

#ifndef jobs_h
#define jobs_h

#import <stdbool.h>
#import <allonet/arr.h>

struct scheduler;


typedef struct job {
    // return true if job should continue. Remember to free your data before returning false.
    bool(*work)(void *data);
    // user data sent to work
    void *data;
} job;

/**
 * A scheduler instance
*/
typedef struct scheduler {
    /// The jobs this scheduler is maintaining
    arr_t(job) jobs;
} scheduler;

/**
 * Add a job to the scheduler
 * @param s The scheduler
 * @param data User data that is sent to the work callback
 * @param work A pointer to a worker function
*/
void scheduler_init(scheduler *s);

/**
 * Add a job to the scheduler
 * @param s The scheduler
 * @param data User data that is sent to the work callback
 * @param work A pointer to a worker function
*/
void scheduler_add(scheduler *s, void *data, bool(*work)(void *));


/**
 * Remove all jobs.
 */
void scheduler_remove_all(scheduler *s);

/**
 * Run the queued jobs
 * @param s The scheduler
*/
void scheduler_tick(scheduler *s);

#endif /* jobs_h */
