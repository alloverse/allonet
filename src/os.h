#ifndef os_h
#define os_h

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

uint64_t allo_os_time(void);
double allo_os_time_seconds(void);
size_t allo_os_working_dir(char *buffer, size_t size);

#endif /* os_h */
