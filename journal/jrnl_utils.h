#pragma once

#include "usn_reader.hh"
#include <sstream>
#include <map>
#include <algorithm>

std::atomic<bool> g_loading{ true };

struct USNEvent 
{
    std::string name;
    std::string date;
    std::string reason;
    std::string directory;
};

struct USNEntryRender
{
    std::string name;
    std::string date;
    std::string reason;
    std::string directory;
    FILE_ID_128 fileId = {};
    std::vector<USNEvent> events;
};

std::vector<USNEntryRender> g_entries;
std::vector<USNEntryRender> g_filteredEntries;
std::vector<USNEntryRender> g_entriesIndividual;
std::vector<USNEntryRender> g_entriesGrouped;
std::mutex g_entriesMutex;

std::vector<char> g_searchInputBuffer(512, 0);
std::string g_searchText;
bool g_resetScroll = false;

bool g_filterDeleted = false;
bool g_filterRenamedNew = false;
bool g_filterRenamedOld = false;
bool g_filterBasicInfo = false;
bool g_filterStream = false;
bool g_filterDataTruncation = false;

std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), str.data(), size_needed, nullptr, nullptr);
    return str;
}

std::string FileTimeToString(const FILETIME& ft)
{
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

bool evaluate_condition(const std::string& field, const std::string& condition) {
    if (condition.empty()) return true;

    std::vector<std::string> parts;
    std::vector<char> ops;
    size_t start = 0;

    while (start < condition.size()) {
        size_t and_pos = condition.find("&&", start);
        size_t not_pos = condition.find("!!", start);
        size_t or_pos = condition.find("||", start);
        size_t min_pos = condition.size();
        char op = 0;
        if (and_pos < min_pos) { min_pos = and_pos; op = '&'; }
        if (not_pos < min_pos) { min_pos = not_pos; op = '!'; }
        if (or_pos < min_pos) { min_pos = or_pos; op = '|'; }
        std::string part = condition.substr(start, min_pos - start);
        parts.push_back(part);
        if (min_pos == condition.size()) break;
        ops.push_back(op);
        start = min_pos + 2;
    }

    if (parts.empty()) return true;

    std::string field_lower = field;
    std::transform(field_lower.begin(), field_lower.end(), field_lower.begin(), ::tolower);

    std::string part_lower = parts[0];
    std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(), ::tolower);

    bool result = field_lower.find(part_lower) != std::string::npos;
    for (size_t i = 0; i < ops.size(); ++i) 
    {
        part_lower = parts[i + 1];
        std::transform(part_lower.begin(), part_lower.end(), part_lower.begin(), ::tolower);

        bool has = field_lower.find(part_lower) != std::string::npos;
        if (ops[i] == '&') result &= has;
        else if (ops[i] == '!') result &= !has;
        else if (ops[i] == '|') result |= has;
    }
    return result;
}

bool matches_search_advanced(const USNEntryRender& e, const std::string& search)
{
    if (search.empty()) return true;

    std::vector<std::string> filters;
    std::stringstream ss(search);
    std::string token;
    while (std::getline(ss, token, ';')) {
        filters.push_back(token);
    }

    std::map<std::string, std::string> column_map;
    column_map["name"] = e.name;
    column_map["reason"] = e.reason;
    column_map["directory"] = e.directory;
    column_map["date"] = e.date;

    for (const std::string& filter : filters) {
        size_t colon = filter.find(':');
        std::string col = "name";
        std::string val = filter;
        if (colon != std::string::npos) {
            col = filter.substr(0, colon);
            val = filter.substr(colon + 1);
        }
        if (column_map.find(col) == column_map.end()) continue;
        if (!evaluate_condition(column_map[col], val)) return false;
    }
    return true;
}

void LoadUSNJournal(const std::wstring& volume)
{
    Run(volume);

    auto rawEntries = GetEntriesCopy();
    g_totalEntries = (int)rawEntries.size();

    std::vector<USNEntryRender> entriesIndividual;
    entriesIndividual.reserve(rawEntries.size());

    std::vector<USNEntryRender> entriesGrouped;
    entriesGrouped.reserve(rawEntries.size());

    std::unordered_map<std::string, int> fileIdMap;

    for (auto& e : rawEntries)
    {
        FILE_ID_128 zeroId = {};
        if (memcmp(&e.fileId, &zeroId, sizeof(FILE_ID_128)) == 0) 
        {
            memcpy(&e.fileId.Identifier, &e.usn, sizeof(ULONGLONG));
        }

        USNEntryRender ind;
        ind.name = WStringToString(e.name);
        ind.date = FileTimeToString(e.date);
        ind.reason = e.reason;
        ind.directory = WStringToString(e.directory);
        ind.fileId = e.fileId;
        entriesIndividual.push_back(std::move(ind));

        USNEntryRender grp;
        grp.name = WStringToString(e.name);
        grp.date = FileTimeToString(e.date);
        grp.reason = e.reason;
        grp.directory = WStringToString(e.directory);
        grp.fileId = e.fileId;

        std::string key((char*)&grp.fileId, sizeof(FILE_ID_128));
        auto it = fileIdMap.find(key);
        if (it != fileIdMap.end()) {
            entriesGrouped[it->second].events.push_back({
                grp.name,
                grp.date,
                grp.reason,
                grp.directory
                });
        }
        else {
            grp.events.push_back({
                grp.name,
                grp.date,
                grp.reason,
                grp.directory
                });

            entriesGrouped.push_back(std::move(grp));
            fileIdMap[key] = (int)(entriesGrouped.size() - 1);
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_entriesMutex);
        g_entriesIndividual = std::move(entriesIndividual);
        g_entriesGrouped = std::move(entriesGrouped);

        g_entries = g_entriesIndividual;
        g_filteredEntries = g_entries;
    }
    g_loading = false;
}

void FilterEntries() {
    std::lock_guard<std::mutex> lock(g_entriesMutex);
    g_filteredEntries.clear();

    bool anyReasonFilterActive = g_filterDeleted || g_filterRenamedNew || g_filterRenamedOld || g_filterBasicInfo || g_filterStream || g_filterDataTruncation;

    for (const auto& e : g_entries)
    {
        bool matches_search = matches_search_advanced(e, g_searchText);
        bool matches_reason = !anyReasonFilterActive;
        if (anyReasonFilterActive) {
            if (g_filterDeleted && e.reason.find("File Delete") != std::string::npos) matches_reason = true;
            if (g_filterRenamedNew && e.reason.find("Rename New Name") != std::string::npos) matches_reason = true;
            if (g_filterRenamedOld && e.reason.find("Rename Old Name") != std::string::npos) matches_reason = true;
            if (g_filterBasicInfo && e.reason.find("Basic Info Change") != std::string::npos) matches_reason = true;
            if (g_filterStream && e.reason.find("Stream Change") != std::string::npos) matches_reason = true;
            if (g_filterDataTruncation && e.reason.find("Data Truncation") != std::string::npos) matches_reason = true;
        }

        if (matches_search && matches_reason) {
            g_filteredEntries.push_back(e);
        }
    }

    std::sort(g_filteredEntries.begin(), g_filteredEntries.end(),
        [](const USNEntryRender& a, const USNEntryRender& b)
        {
            return a.date > b.date;
        });
}