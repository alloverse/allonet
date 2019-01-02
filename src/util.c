//
//  util.c
//  allonet
//
//  Created by Nevyn Bengtsson on 1/2/19.
//

#include "util.h"
#include <time.h>

int64_t
get_ts_mono(void)
{
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return (int64_t)tv.tv_sec * 1000000LL + (tv.tv_nsec / 1000);
}
