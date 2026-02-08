#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "journal/usn_reader.hh"
#include "journal/jrnl_utils.h"
#include "d3d11/d3dx.hh"
#include "_font.h"
#include "journal/usn_info.h"

ImFont* g_font = nullptr;
static int g_modalOpenRow = -1;
static std::string g_usnAnalysisResult = "";
static bool g_showUSNAnalysisModal = false;

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L, 0L,
        hInstance,
        nullptr, nullptr, nullptr, nullptr,
        _T("JournalParser"),
        nullptr
    };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        _T("JournalParser"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, wc.hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.07f, 0.75f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.21f, 0.35f, 0.54f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.39f, 0.64f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.34f, 0.54f, 0.79f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.39f, 0.64f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.29f, 0.49f, 0.74f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.19f, 0.19f, 0.19f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.21f, 0.35f, 0.54f, 0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.39f, 0.64f, 0.90f);

    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ScrollbarSize = 16.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.CellPadding = ImVec2(8, 6);
    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(10, 6);

    ImFont* poppins = io.Fonts->AddFontFromMemoryCompressedTTF(Poppins_Medium_compressed_data, Poppins_Medium_compressed_size, 16.5f);
    io.FontDefault = poppins;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    wchar_t windowsPath[MAX_PATH];
    UINT len = GetWindowsDirectoryW(windowsPath, MAX_PATH);
    if (len == 0 || len > MAX_PATH) {
        CleanupDeviceD3D();
        return 1;
    }
    wchar_t systemDrive[3] = { windowsPath[0], L':', L'\0' };

    std::wstring checkPath = std::wstring(systemDrive) + L"\\Windows\\System32\\kernel32.dll";
    if (std::filesystem::exists(checkPath)) {
        std::thread usnThread(LoadUSNJournal, std::wstring(systemDrive));
        usnThread.detach();
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        RECT rect;
        GetClientRect(hwnd, &rect);
        ImGui::SetNextWindowSize(ImVec2((float)rect.right, (float)rect.bottom));
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("USN Journal", nullptr, window_flags);

        if (g_loading) {
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f - 20.0f);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            float t = (float)ImGui::GetTime();

            int currentItem = g_processedEntries.load();
            int totalItems = g_totalEntries.load();

            std::string subText;
            if (totalItems > 0) {
                char progressBuf[32];
                sprintf_s(progressBuf, "%d/%d", currentItem, totalItems);
                subText = progressBuf;
            }
            else {
                subText = "PROCESSING: " + std::to_string(currentItem) + " ENTRIES";
            }
            std::transform(subText.begin(), subText.end(), subText.begin(), ::toupper);

            float radius = 15.0f;
            float thickness = 4.0f;
            ImVec2 spinnerCenter = ImVec2(center.x, center.y - 48.0f);

            int num_segments = 25;
            float start = abs(sinf(t * 1.8f) * (num_segments - 5));

            float a_min = 3.14159265358979323846f * 2.0f * ((float)start) / (float)num_segments;
            float a_max = 3.14159265358979323846f * 2.0f * ((float)num_segments - 3) / (float)num_segments;

            ImU32 accentCol = IM_COL32(255, 100, 100, 220);

            for (int i = 0; i < num_segments; i++) {
                const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
                draw_list->PathLineTo(ImVec2(spinnerCenter.x + cosf(a + t * -8) * radius, spinnerCenter.y + sinf(a + t * -8) * radius));
            }
            draw_list->PathStroke(accentCol, false, thickness);

            const char* loadingText = "Parsing USN Journal";
            float fontSize = 17.5f;
            float defaultFontSize = 16.5f;
            float fontScale = fontSize / defaultFontSize;

            ImVec2 textSize = ImGui::CalcTextSize(loadingText);
            textSize.x *= fontScale;
            textSize.y *= fontScale;
            ImVec2 textPos = ImVec2(center.x - textSize.x * 0.5f, center.y - 20.0f);
            ImU32 textCol = IM_COL32(255, 255, 255, 255);
            draw_list->AddText(ImGui::GetFont(), fontSize, textPos, textCol, loadingText);

            ImVec2 subSize = ImGui::CalcTextSize(subText.c_str());
            subSize.x *= fontScale;
            subSize.y *= fontScale;
            ImVec2 subPos = ImVec2(center.x - subSize.x * 0.5f, textPos.y + textSize.y + 15.0f);
            ImU32 subTextCol = IM_COL32(153, 153, 153, 255);
            draw_list->AddText(ImGui::GetFont(), fontSize, subPos, subTextCol, subText.c_str());
        }
        else {
            ImGui::PushItemWidth(300.0f);
            bool searchEntered = ImGui::InputTextWithHint("##Search", "Search", g_searchInputBuffer.data(), g_searchInputBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            bool filterChanged = false;
            filterChanged |= ImGui::Checkbox("Deleted", &g_filterDeleted);
            ImGui::SameLine();
            filterChanged |= ImGui::Checkbox("Renamed New", &g_filterRenamedNew);
            ImGui::SameLine();
            filterChanged |= ImGui::Checkbox("Renamed Old", &g_filterRenamedOld);
            ImGui::SameLine();
            filterChanged |= ImGui::Checkbox("Basic Info Change", &g_filterBasicInfo);
            ImGui::SameLine();
            filterChanged |= ImGui::Checkbox("Stream Change", &g_filterStream);
            ImGui::SameLine();
            filterChanged |= ImGui::Checkbox("Data Truncation", &g_filterDataTruncation);

            ImGui::SameLine();
            float buttonWidth = ImGui::CalcTextSize("USN Deleted").x + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttonWidth);
            if (ImGui::Button("USN Deleted")) {
                USNAnalysis analyzer;
                g_usnAnalysisResult = analyzer.analyze_usn_status();
                g_showUSNAnalysisModal = true;
            }

            if (g_showUSNAnalysisModal) {
                ImGui::OpenPopup("USN Deleted Result");
            }

            ImGui::SetNextWindowSize(ImVec2(1000, 300), ImGuiCond_FirstUseEver);
            if (ImGui::BeginPopupModal("USN Deleted Result", &g_showUSNAnalysisModal))
            {
                ImGui::TextWrapped("%s", g_usnAnalysisResult.c_str());
                ImGui::EndPopup();
            }

            if (searchEntered)
            {
                g_searchText = std::string(g_searchInputBuffer.data());
                FilterEntries();
                g_resetScroll = true;
            }
            if (filterChanged)
            {
                FilterEntries();
                g_resetScroll = true;
            }

            std::lock_guard<std::mutex> lock(g_entriesMutex);
            auto& displayEntries = g_filteredEntries;

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("TableScrollArea", ImVec2(0, -25.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PopStyleVar();

            if (ImGui::BeginTable("USNTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
                    if (sortSpecs->SpecsDirty) {
                        const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                        std::sort(displayEntries.begin(), displayEntries.end(),
                            [&](const USNEntryRender& a, const USNEntryRender& b) {
                                switch (spec.ColumnIndex) {
                                case 0: return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (a.name < b.name) : (a.name > b.name);
                                case 1: return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (a.date < b.date) : (a.date > b.date);
                                case 2: return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (a.reason < b.reason) : (a.reason > b.reason);
                                case 3: return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (a.directory < b.directory) : (a.directory > b.directory);
                                default: return false;
                                }
                            });
                        sortSpecs->SpecsDirty = false;
                    }
                }

                if (g_resetScroll) {
                    ImGui::SetScrollY(0.0f);
                    g_resetScroll = false;
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(displayEntries.size()));
                while (clipper.Step()) {
                    for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++) {
                        const auto& e = displayEntries[row_n];
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.name.c_str());
                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(e.date.c_str());
                        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e.reason.c_str());
                        ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e.directory.c_str());

                        ImGui::PushID(row_n);

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Selectable("##row_full", false, ImGuiSelectableFlags_SpanAllColumns);

                        if (ImGui::BeginPopupContextItem(("row_options_" + std::to_string(row_n)).c_str())) {
                            if (ImGui::Selectable("Copy Name")) {
                                const std::wstring wname(e.name.begin(), e.name.end());
                                if (OpenClipboard(NULL)) {
                                    EmptyClipboard();
                                    size_t sizeInBytes = (wname.size() + 1) * sizeof(wchar_t);
                                    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
                                    if (hGlob) {
                                        void* pGlob = GlobalLock(hGlob);
                                        memcpy(pGlob, wname.c_str(), sizeInBytes);
                                        GlobalUnlock(hGlob);
                                        SetClipboardData(CF_UNICODETEXT, hGlob);
                                    }
                                    CloseClipboard();
                                }
                            }

                            if (ImGui::Selectable("Copy Path")) {
                                const std::wstring wpath(e.directory.begin(), e.directory.end());
                                if (OpenClipboard(NULL)) {
                                    EmptyClipboard();
                                    size_t sizeInBytes = (wpath.size() + 1) * sizeof(wchar_t);
                                    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
                                    if (hGlob) {
                                        void* pGlob = GlobalLock(hGlob);
                                        memcpy(pGlob, wpath.c_str(), sizeInBytes);
                                        GlobalUnlock(hGlob);
                                        SetClipboardData(CF_UNICODETEXT, hGlob);
                                    }
                                    CloseClipboard();
                                }
                            }

                            if (ImGui::Selectable("Open Path")) {
                                const std::wstring wpath(e.directory.begin(), e.directory.end());
                                ShellExecuteW(NULL, L"explore", wpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            }

                            if (ImGui::Selectable("Show File Information")) {
                                g_modalOpenRow = row_n;
                            }

                            ImGui::EndPopup();
                        }

                        if (g_modalOpenRow == row_n) {
                            auto fileId = e.fileId;
                            auto it = std::find_if(g_entriesGrouped.begin(), g_entriesGrouped.end(),
                                [fileId](const USNEntryRender& r) { return memcmp(&r.fileId, &fileId, sizeof(FILE_ID_128)) == 0; });
                            if (it != g_entriesGrouped.end()) {
                                auto& grouped = *it;

                                static bool showModal = true;
                                if (showModal) {
                                    ImGui::OpenPopup(("Entry Information - " + grouped.name).c_str());
                                }

                                ImGui::SetNextWindowSize(ImVec2(1300, 600), ImGuiCond_FirstUseEver);
                                ImGui::SetNextWindowSizeConstraints(ImVec2(800, 400), ImVec2(FLT_MAX, FLT_MAX));
                                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
                                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.98f));

                                if (ImGui::BeginPopupModal(("Entry Information - " + grouped.name).c_str(), &showModal,
                                    ImGuiWindowFlags_None)) {
                                    if (!grouped.events.empty()) {
                                        const auto& ev = grouped.events.back();
                                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                                        ImGui::TextUnformatted("Latest Event:");
                                        ImGui::PopStyleColor();
                                        ImGui::Indent(10.0f);
                                        ImGui::Text("Date: %s", ev.date.c_str());
                                        ImGui::Text("Reason: %s", ev.reason.c_str());
                                        ImGui::Text("Directory: %s", ev.directory.c_str());
                                        ImGui::Unindent(10.0f);
                                    }

                                    ImGui::Separator();

                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                                    ImGui::TextUnformatted("Event History:");
                                    ImGui::PopStyleColor();

                                    if (ImGui::BeginTable("agg_events_table", 4,
                                        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                                        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                                        ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch);
                                        ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthStretch);
                                        ImGui::TableHeadersRow();

                                        for (auto it = grouped.events.rbegin(); it != grouped.events.rend(); ++it) {
                                            const auto& ev = *it;
                                            ImGui::TableNextRow();
                                            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(ev.name.c_str());
                                            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ev.date.c_str());
                                            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(ev.reason.c_str());
                                            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(ev.directory.c_str());
                                        }
                                        ImGui::EndTable();
                                    }

                                    ImGui::EndPopup();
                                }
                                else {
                                    g_modalOpenRow = -1;
                                    showModal = true;
                                }

                                ImGui::PopStyleColor();
                                ImGui::PopStyleVar(2);
                            }
                        }
                        ImGui::PopID();
                    }
                }
                clipper.End();

                ImGui::EndTable();
            }

            ImGui::EndChild();

            std::string oldestDate = "N/A";
            if (!g_entries.empty()) {
                oldestDate = std::min_element(
                    g_entries.begin(), g_entries.end(),
                    [](const USNEntryRender& a, const USNEntryRender& b) {
                        return a.date < b.date;
                    }
                )->date;
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Oldest Entry: %s", oldestDate.c_str());
            }

            ImGui::End();
            ImGui::Render();

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            float clear_color[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}