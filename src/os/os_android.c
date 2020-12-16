#include "../os.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#define NS_PER_SEC 1000000000ULL

uint64_t allo_os_time() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * NS_PER_SEC + (uint64_t) t.tv_nsec;
}

double allo_os_time_seconds() {
    return allo_os_time() / (double) NS_PER_SEC;
}

size_t allo_os_working_dir(char* buffer, size_t size) {
    return getcwd(buffer, size) ? strlen(buffer) : 0;
}
