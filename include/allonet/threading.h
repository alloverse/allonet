#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #include <threads.h>
#else
    #include <tinycthread.h>
#endif
