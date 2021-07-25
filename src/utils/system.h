#pragma once

#ifdef _WIN32

#include <windows.h>

inline std::int64_t get_total_system_memory()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return std::int64_t(status.ullTotalPhys);
}

#else

#include <unistd.h>
#include <iostream>

inline std::int64_t get_total_system_memory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return std::int64_t(pages) * std::int64_t(page_size);
}

#endif
