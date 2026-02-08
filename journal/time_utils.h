#include <format>
#include <windows.h>
#include <string>

namespace time_utils {
    std::wstring filetime_to_string(const FILETIME& ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

        return std::format(L"{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
            stLocal.wYear, stLocal.wMonth, stLocal.wDay,
            stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    }

    FILETIME get_boot_time() {
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        ULONGLONG nowTicks = (static_cast<ULONGLONG>(ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;
        ULONGLONG uptimeMs = GetTickCount64();
        ULONGLONG bootTicks = nowTicks - uptimeMs * 10000ULL;

        FILETIME ftBoot{
            static_cast<DWORD>(bootTicks & 0xFFFFFFFF),
            static_cast<DWORD>(bootTicks >> 32)
        };

        return ftBoot;
    }
}