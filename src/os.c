#if defined(_WIN32)
    #define PLATFORM_NAME "windows"
    #include "os/os_win32.c"
#elif defined(_WIN64)
    #define PLATFORM_NAME "windows"
    #include "os/os_win32.c"
#elif defined(__CYGWIN__) && !defined(_WIN32)
    #define PLATFORM_NAME "windows"
    #include "os/os_win32.c"
#elif defined(__ANDROID__)
    #define PLATFORM_NAME "android"
    #include "os/os_android.c"
#elif defined(__linux__)
    #define PLATFORM_NAME "linux"
    #include "os/os_linux.c"
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
    #include <sys/param.h>
    #if defined(BSD)
        #define PLATFORM_NAME "bsd" // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
    #endif
#elif defined(__hpux)
    #define PLATFORM_NAME "hp-ux" // HP-UX
#elif defined(_AIX)
    #define PLATFORM_NAME "aix" // IBM AIX
#elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)
    #include <TargetConditionals.h>
    #include "os/os_macos.c"
    #if TARGET_IPHONE_SIMULATOR == 1
        #define PLATFORM_NAME "ios" // Apple iOS
    #elif TARGET_OS_IPHONE == 1
        #define PLATFORM_NAME "ios" // Apple iOS
    #elif TARGET_OS_MAC == 1
        #define PLATFORM_NAME "osx" // Apple OSX
    #endif
#elif defined(__sun) && defined(__SVR4)
    #define PLATFORM_NAME "solaris" // Oracle Solaris, Open Indiana
#else
    #define PLATFORM_NAME NULL
    #error Unsupported OS
#endif
