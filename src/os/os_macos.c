#include "../os.h"
#include <mach/mach_time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

uint64_t allo_os_time() {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    uint64_t frequency = (info.denom * 1e6) / info.numer;
    return mach_absolute_time() / frequency;
}

double allo_os_time_seconds() {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    uint64_t frequency = (info.denom * 1e9) / info.numer;
    return mach_absolute_time() / (double)frequency;
}

size_t allo_os_working_dir(char* buffer, size_t size) {
    return getcwd(buffer, size) ? strlen(buffer) : 0;
}
