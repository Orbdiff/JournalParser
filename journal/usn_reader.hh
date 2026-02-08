#pragma once
#define _WIN32_WINNT 0x0A00

#include <Windows.h>
#include <winioctl.h>
#include <cstdio>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <format>
#include <iostream>
#include <thread>
#include <mutex>
#include <shellapi.h>
#include <atomic>

struct USNEntry {
    FILE_ID_128 fileId;
    ULONGLONG usn;
    std::wstring name;
    FILETIME date;
    std::string reason;
    std::wstring directory;
};

struct FileEvent {
    FILETIME date;
    std::string reason;
};

struct AggregatedUSNEntry {
    std::wstring name;
    std::wstring directory;
    FILE_ID_128 fileId = {};
    std::vector<FileEvent> events;
};

inline bool operator==(const FILE_ID_128& a, const FILE_ID_128& b) {
    return memcmp(&a, &b, sizeof(FILE_ID_128)) == 0;
}

std::wstring volumeLetter_;
HANDLE volumeHandle_ = INVALID_HANDLE_VALUE;
BYTE* buffer_ = nullptr;
USN_JOURNAL_DATA_V0 journalData_{};
std::unordered_map<std::string, std::wstring> pathCache_;
std::mutex cacheMutex_;
std::mutex entriesMutex_;
std::vector<USNEntry> entries;

std::atomic<int> g_processedEntries = 0;
std::atomic<int> g_totalEntries = 0;

std::string ReasonToString(DWORD reason) {
    struct ReasonFlag { DWORD flag; const char* desc; };
    static const ReasonFlag reasons[] = {
        {USN_REASON_DATA_OVERWRITE, "Data Overwrite"},
        {USN_REASON_DATA_EXTEND, "Data Extend"},
        {USN_REASON_DATA_TRUNCATION, "Data Truncation"},
        {USN_REASON_NAMED_DATA_OVERWRITE, "Named Data Overwrite"},
        {USN_REASON_NAMED_DATA_EXTEND, "Named Data Extend"},
        {USN_REASON_NAMED_DATA_TRUNCATION, "Named Data Truncation"},
        {USN_REASON_FILE_CREATE, "File Create"},
        {USN_REASON_FILE_DELETE, "File Delete"},
        {USN_REASON_EA_CHANGE, "EA Change"},
        {USN_REASON_SECURITY_CHANGE, "Security Change"},
        {USN_REASON_RENAME_OLD_NAME, "Rename Old Name"},
        {USN_REASON_RENAME_NEW_NAME, "Rename New Name"},
        {USN_REASON_INDEXABLE_CHANGE, "Indexable Change"},
        {USN_REASON_BASIC_INFO_CHANGE, "Basic Info Change"},
        {USN_REASON_HARD_LINK_CHANGE, "Hard Link Change"},
        {USN_REASON_COMPRESSION_CHANGE, "Compression Change"},
        {USN_REASON_ENCRYPTION_CHANGE, "Encryption Change"},
        {USN_REASON_OBJECT_ID_CHANGE, "Object ID Change"},
        {USN_REASON_REPARSE_POINT_CHANGE, "Reparse Point Change"},
        {USN_REASON_STREAM_CHANGE, "Stream Change"},
        {USN_REASON_TRANSACTED_CHANGE, "Transacted Change"},
        {USN_REASON_INTEGRITY_CHANGE, "Integrity Change"},
        {USN_REASON_CLOSE, "Close"}
    };

    std::string result;
    for (const auto& r : reasons)
        if (reason & r.flag) {
            if (!result.empty()) result += " | ";
            result += r.desc;
        }

    return result.empty() ? "?" : result;
}

std::wstring GetDirectoryById(ULONGLONG fileId) {
    std::string key = std::to_string(fileId);
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = pathCache_.find(key);
        if (it != pathCache_.end()) return it->second;
    }

    FILE_ID_DESCRIPTOR desc{};
    desc.dwSize = sizeof(desc);
    desc.Type = FileIdType;
    desc.FileId.QuadPart = (LONGLONG)fileId;

    HANDLE fileHandle = OpenFileById(volumeHandle_, &desc, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, FILE_FLAG_BACKUP_SEMANTICS);

    std::wstring directory = L"?";
    if (fileHandle != INVALID_HANDLE_VALUE) {
        WCHAR path[MAX_PATH] = {};
        DWORD ret = GetFinalPathNameByHandleW(fileHandle, path, MAX_PATH, FILE_NAME_NORMALIZED);
        CloseHandle(fileHandle);

        if (ret > 0 && ret < MAX_PATH) {
            std::wstring fullPath(path);
            if (fullPath.rfind(L"\\\\?\\", 0) == 0)
                fullPath = fullPath.substr(4);
            directory = fullPath;
        }
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        pathCache_[key] = directory;
    }

    return directory;
}

std::wstring GetDirectoryById(const FILE_ID_128& fileId128) {
    std::string key((char*)&fileId128, sizeof(FILE_ID_128));
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = pathCache_.find(key);
        if (it != pathCache_.end()) return it->second;
    }

    FILE_ID_DESCRIPTOR desc{};
    desc.dwSize = sizeof(desc);
#if (_WIN32_WINNT >= 0x0602)
    desc.Type = ExtendedFileIdType;
    desc.ExtendedFileId = fileId128;
#else
    return L"[Unsupported: FILE_ID_128]";
#endif

    HANDLE fileHandle = OpenFileById(volumeHandle_, &desc, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, FILE_FLAG_BACKUP_SEMANTICS);

    std::wstring directory = L"?";
    if (fileHandle != INVALID_HANDLE_VALUE) {
        WCHAR path[MAX_PATH] = {};
        DWORD ret = GetFinalPathNameByHandleW(fileHandle, path, MAX_PATH, FILE_NAME_NORMALIZED);
        CloseHandle(fileHandle);

        if (ret > 0 && ret < MAX_PATH) {
            std::wstring fullPath(path);
            if (fullPath.rfind(L"\\\\?\\", 0) == 0)
                fullPath = fullPath.substr(4);
            directory = fullPath;
        }
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        pathCache_[key] = directory;
    }

    return directory;
}

void PushEntry(const FILE_ID_128& fileId, ULONGLONG usn, const std::wstring& name,
    const FILETIME& date, const std::string& reason, const std::wstring& dir) {
    USNEntry entry{ fileId, usn, name, date, reason, dir };
    std::lock_guard<std::mutex> lock(entriesMutex_);
    entries.push_back(entry);
}

void Cleanup() {
    if (buffer_) {
        VirtualFree(buffer_, 0, MEM_RELEASE);
        buffer_ = nullptr;
    }
    if (volumeHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(volumeHandle_);
        volumeHandle_ = INVALID_HANDLE_VALUE;
    }
}

bool OpenVolume() {
    std::wstring devicePath = L"\\\\.\\" + volumeLetter_;
    volumeHandle_ = CreateFileW(devicePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    return volumeHandle_ != INVALID_HANDLE_VALUE;
}

bool QueryJournal() {
    DWORD bytesReturned = 0;
    return DeviceIoControl(volumeHandle_, FSCTL_QUERY_USN_JOURNAL, nullptr, 0,
        &journalData_, sizeof(journalData_), &bytesReturned, nullptr);
}

bool AllocateBuffer() {
    const DWORD bufferSize = 32 * 1024 * 1024;
    buffer_ = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return buffer_ != nullptr;
}

bool Dump() {
    if (!OpenVolume() || !QueryJournal() || !AllocateBuffer())
        return false;

    READ_USN_JOURNAL_DATA_V0 readData{};
    readData.StartUsn = journalData_.FirstUsn;
    readData.ReasonMask = 0xFFFFFFFF;
    readData.UsnJournalID = journalData_.UsnJournalID;

    const DWORD bufferSize = 32 * 1024 * 1024;
    DWORD bytesReturned = 0;

    while (DeviceIoControl(volumeHandle_, FSCTL_READ_USN_JOURNAL, &readData, sizeof(readData),
        buffer_, bufferSize, &bytesReturned, nullptr)) {
        if (bytesReturned <= sizeof(USN))
            break;

        BYTE* ptr = buffer_ + sizeof(USN);
        BYTE* end = buffer_ + bytesReturned;

        while (ptr < end) {
            auto common = reinterpret_cast<USN_RECORD_COMMON_HEADER*>(ptr);
            if (common->RecordLength == 0) break;

            std::wstring name = L"?";
            std::wstring directory = L"?";
            FILETIME ft{}, localTime{};
            std::string reasonStr;
            ULONGLONG usn = 0;
            FILE_ID_128 fileId = {};

            if (common->MajorVersion == 2) {
                auto rec = reinterpret_cast<USN_RECORD_V2*>(ptr);
                name.assign(reinterpret_cast<WCHAR*>((BYTE*)rec + rec->FileNameOffset),
                    rec->FileNameLength / sizeof(WCHAR));
                directory = GetDirectoryById(rec->ParentFileReferenceNumber);
                ft.dwLowDateTime = rec->TimeStamp.LowPart;
                ft.dwHighDateTime = rec->TimeStamp.HighPart;
                FileTimeToLocalFileTime(&ft, &localTime);
                reasonStr = ReasonToString(rec->Reason);
                usn = rec->Usn;
                memcpy(&fileId.Identifier, &rec->FileReferenceNumber, sizeof(ULONGLONG));
                memset(&fileId.Identifier[8], 0, 8);
            }
            else if (common->MajorVersion == 3) {
                auto rec = reinterpret_cast<USN_RECORD_V3*>(ptr);
                name.assign(reinterpret_cast<WCHAR*>((BYTE*)rec + rec->FileNameOffset),
                    rec->FileNameLength / sizeof(WCHAR));
                directory = GetDirectoryById(rec->ParentFileReferenceNumber);
                ft.dwLowDateTime = rec->TimeStamp.LowPart;
                ft.dwHighDateTime = rec->TimeStamp.HighPart;
                FileTimeToLocalFileTime(&ft, &localTime);
                reasonStr = ReasonToString(rec->Reason);
                usn = rec->Usn;
                fileId = rec->FileReferenceNumber;
            }
            else if (common->MajorVersion == 4) {
                auto rec = reinterpret_cast<USN_RECORD_V4*>(ptr);
                directory = GetDirectoryById(rec->FileReferenceNumber);
                size_t last_slash = directory.find_last_of(L"\\");
                if (last_slash != std::string::npos && last_slash + 1 < directory.size()) {
                    name = directory.substr(last_slash + 1);
                }
                else {
                    name = L"[Requires lookup]";
                }
                ft.dwLowDateTime = 0;
                ft.dwHighDateTime = 0;
                FileTimeToLocalFileTime(&ft, &localTime);
                reasonStr = ReasonToString(rec->Reason);
                usn = rec->Usn;
                fileId = rec->FileReferenceNumber;
            }

            PushEntry(fileId, usn, name, localTime, reasonStr, directory);
            g_processedEntries++;
            ptr += common->RecordLength;
        }

        readData.StartUsn = *(USN*)buffer_;
    }

    Cleanup();
    return true;
}

std::vector<USNEntry> GetEntriesCopy() {
    std::lock_guard<std::mutex> lock(entriesMutex_);
    return entries;
}

std::vector<AggregatedUSNEntry> EventsFileID() {
    std::unordered_map<std::string, AggregatedUSNEntry> aggMap;

    for (auto& entry : GetEntriesCopy()) {
        std::string key((char*)&entry.fileId, sizeof(FILE_ID_128));
        auto it = aggMap.find(key);
        if (it == aggMap.end()) {
            AggregatedUSNEntry agg;
            agg.fileId = entry.fileId;
            agg.name = entry.name;
            agg.directory = entry.directory;
            agg.events.push_back({ entry.date, entry.reason });
            aggMap[key] = agg;
        }
        else {
            it->second.events.push_back({ entry.date, entry.reason });
        }
    }

    std::vector<AggregatedUSNEntry> result;
    for (auto& [_, agg] : aggMap)
        result.push_back(agg);

    return result;
}

void Run(const std::wstring& volumeLetter) {
    volumeLetter_ = volumeLetter;
    Dump();
}