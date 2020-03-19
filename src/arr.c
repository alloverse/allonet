#include "allonet/arr.h"
#include <assert.h>
#include <stdlib.h>

void __arr_reserve(void** data, size_t n, size_t* capacity, size_t stride) {
  if (*capacity == 0) {
    *capacity = 1;
  }

  while (*capacity < n) {
    *capacity <<= 1;
    assert(*capacity > 0 && "Out of memory");
  }

  *data = realloc(*data, *capacity * stride);
  assert(*data && "Out of memory");
}
