#pragma once

#include "usn_reader.hh"

std::atomic<bool> g_loading{ true };

struct USNEvent {
    std::string name;
    std::string date;
    std::string reason;
    std::string directory;
};

struct USNEntryRender {
    std::string name;
    std::string date;
    std::string reason;
    std::string directory;
    ULONGLONG fileId = 0;
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

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), str.data(), size_needed, nullptr, nullptr);
    return str;
}

std::string FileTimeToString(const FILETIME& ft) {
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

void LoadUSNJournal(const std::wstring& volume) {
    USNJournalReader reader(volume);
    reader.Run();

    auto rawEntries = reader.GetEntriesCopy();

    std::vector<USNEntryRender> entriesIndividual;
    entriesIndividual.reserve(rawEntries.size());

    std::vector<USNEntryRender> entriesGrouped;
    entriesGrouped.reserve(rawEntries.size());

    std::unordered_map<ULONGLONG, size_t> fileIdMap;

    for (auto& e : rawEntries) {
        if (e.fileId == 0)
            e.fileId = e.usn;

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

        auto it = fileIdMap.find(grp.fileId);
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

            ULONGLONG id = grp.fileId;
            entriesGrouped.push_back(std::move(grp));
            fileIdMap[id] = entriesGrouped.size() - 1;
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

    if (g_searchText.empty()) {
        g_filteredEntries = g_entries;
    }
    else {
        std::vector<std::string> conditions;
        size_t start = 0, end;
        while ((end = g_searchText.find(';', start)) != std::string::npos) {
            conditions.push_back(g_searchText.substr(start, end - start));
            start = end + 1;
        }
        conditions.push_back(g_searchText.substr(start));

        for (auto& e : g_entries) {
            bool match = true;
            for (auto& cond : conditions) {
                size_t colon = cond.find(':');
                std::string col = "any";
                std::string val;
                if (colon != std::string::npos) {
                    col = cond.substr(0, colon);
                    val = cond.substr(colon + 1);
                }
                else val = cond;

                std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                std::string nameLower = e.name; std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::string reasonLower = e.reason; std::transform(reasonLower.begin(), reasonLower.end(), reasonLower.begin(), ::tolower);
                std::string dirLower = e.directory; std::transform(dirLower.begin(), dirLower.end(), dirLower.begin(), ::tolower);

                bool conditionMatch = false;
                if (col == "name") conditionMatch = nameLower.find(val) != std::string::npos;
                else if (col == "reason") conditionMatch = reasonLower.find(val) != std::string::npos;
                else if (col == "directory") conditionMatch = dirLower.find(val) != std::string::npos;
                else conditionMatch = (nameLower.find(val) != std::string::npos ||
                    reasonLower.find(val) != std::string::npos ||
                    dirLower.find(val) != std::string::npos);

                if (!conditionMatch) { match = false; break; }
            }
            if (match) g_filteredEntries.push_back(e);
        }
    }

    std::sort(g_filteredEntries.begin(), g_filteredEntries.end(),
        [](const USNEntryRender& a, const USNEntryRender& b) {
            return a.date > b.date;
        });
}