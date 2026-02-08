#pragma once

#include "time_utils.h"
#include <iostream>
#include <windows.h>
#include <string>

struct USNAnalysis {
    static constexpr std::wstring_view USN_PATH = L"C:\\$Extend\\$UsnJrnl:$J";

    static std::string wstring_to_string(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
        return str;
    }

    bool get_usn_creation_time(FILETIME& outCreationTime) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(USN_PATH.data(), GetFileExInfoStandard, &fad)) {
            return false;
        }
        outCreationTime = fad.ftCreationTime;
        return true;
    }

    std::string analyze_usn_status() {
        FILETIME ftCreation{};
        if (!get_usn_creation_time(ftCreation)) {
            DWORD err = GetLastError();
            std::string errorMsg = "[!] Failed to retrieve USN Journal creation time.\n";
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                errorMsg += "[!] $UsnJrnl:$J not found.\n";
            }
            else {
                errorMsg += "[!] Error code: " + std::to_string(err) + "\n";
            }
            return errorMsg;
        }

        FILETIME ftBoot = time_utils::get_boot_time();

        ULONGLONG bootTicks = (static_cast<ULONGLONG>(ftBoot.dwHighDateTime) << 32) | ftBoot.dwLowDateTime;
        ULONGLONG createTicks = (static_cast<ULONGLONG>(ftCreation.dwHighDateTime) << 32) | ftCreation.dwLowDateTime;

        std::string result = " Checking USN Journal at path: " + wstring_to_string(std::wstring(USN_PATH.begin(), USN_PATH.end())) + "\n";
        result += " USN Journal creation time: " + wstring_to_string(time_utils::filetime_to_string(ftCreation)) + "\n";
        result += " System boot time: " + wstring_to_string(time_utils::filetime_to_string(ftBoot)) + "\n";

        if (createTicks > bootTicks) {
            result += "\n $UsnJrnl:$J has a creation date later than the system boot, it is very likely that the USN Journal was deleted.\n";
            result += "   Explanation: The usnjrnl creation time (" + wstring_to_string(time_utils::filetime_to_string(ftCreation)) + ") is AFTER the boot time (" + wstring_to_string(time_utils::filetime_to_string(ftBoot)) + "), which suggests manipulation or deletion.\n";
        }
        else {
            result += "\n $UsnJrnl:$J Intact!\n";
            result += "   Explanation: The usnjrnl creation time (" + wstring_to_string(time_utils::filetime_to_string(ftCreation)) + ") is BEFORE the boot time (" + wstring_to_string(time_utils::filetime_to_string(ftBoot)) + "), indicating no recent changes or deletions.\n";
        }

        return result;
    }
};