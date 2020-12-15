#include "../os.h"
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <stdio.h>

uint64_t allo_os_time() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    //TODO: this is probably microseconds rather than milliseconds but I don't have a windows to try it on...
    return t.QuadPart;
}

double allo_os_time_seconds() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    
    return t.QuadPart / (double)f.QuadPart;
}

size_t allo_os_working_dir(char* buffer, size_t size) {
    WCHAR wpath[1024];
    int length = GetCurrentDirectoryW((int) size, wpath);
    if (length) {
        return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
    }
    return 0;
}
