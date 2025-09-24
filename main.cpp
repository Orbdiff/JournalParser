#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

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

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "usn_reader.hh"
#include "jrnl_utils.h"
#include "d3dx.hh"

#pragma comment(lib, "d3d11.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
        hInstance, nullptr, nullptr, nullptr, nullptr,
        _T("JournalParser"), nullptr };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName, _T("JournalParser"),
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); return 1; }
    CreateRenderTarget();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.29f, 0.29f, 0.30f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.31f, 0.33f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.26f, 0.28f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.19f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.09f, 0.50f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.30f, 0.45f, 0.70f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.40f, 0.55f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.30f, 0.40f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.35f, 0.45f, 1.00f);

    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.40f, 0.45f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.25f, 0.30f, 0.70f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.35f, 0.40f, 0.80f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.45f, 0.50f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.30f, 0.37f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);

    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.13f, 0.25f);

    style.FrameRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ScrollbarSize = 14.0f;

    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.CellPadding = ImVec2(6, 4);
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(10, 6);

    ImFont* arialFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 14.5f);
    io.FontDefault = arialFont;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    std::thread usnThread(LoadUSNJournal, L"C:");
    usnThread.detach();

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

        if (g_loading)
        {
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

            float radius = 30.0f; 
            int dotCount = 12;
            float dotRadius = 5.0f;
            float speed = 2.5f;
            float time = (float)ImGui::GetTime();

            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            for (int i = 0; i < dotCount; i++)
            {
                float angle = (IM_PI * 2.0f / dotCount) * i + time * speed;
                float x = center.x + cosf(angle) * radius;
                float y = center.y + sinf(angle) * radius;

                float alpha = (sinf(time * speed + i * 0.5f) * 0.5f + 0.5f);
                int a = (int)(100 + 155 * alpha);

                draw_list->AddCircleFilled(ImVec2(x, y), dotRadius, IM_COL32(0, 150, 255, a));
            }

            static const char* messages[] = {
                "Parsing USN Journal...",
                "Finalizing parsing..."
            };
            const int numMessages = sizeof(messages) / sizeof(messages[0]);
            int index = static_cast<int>(time / 10.0f) % numMessages;

            float fontSize = 18.0f;
            ImVec2 textSize = ImGui::CalcTextSize(messages[index]);
            ImVec2 textPos(center.x - textSize.x * 0.5f, center.y + radius + 25.0f);
            draw_list->AddText(ImGui::GetFont(), fontSize, textPos, IM_COL32(200, 200, 200, 255), messages[index]);
        } else {
            ImGui::PushItemWidth(600.0f);
            if (ImGui::InputText("Search", g_searchInputBuffer.data(), g_searchInputBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                g_searchText = std::string(g_searchInputBuffer.data());
                FilterEntries();
                g_resetScroll = true;
            }
            ImGui::PopItemWidth();

            std::string oldestDate = "N/A";
            {
                std::lock_guard<std::mutex> lock(g_entriesMutex);
                if (!g_entries.empty()) {
                    oldestDate = std::min_element(
                        g_entries.begin(), g_entries.end(),
                        [](const USNEntryRender& a, const USNEntryRender& b) {
                            return a.date < b.date;
                        }
                    )->date;
                }
            }

            float windowWidth = ImGui::GetWindowContentRegionMax().x;
            ImVec2 textSize = ImGui::CalcTextSize(("Oldest Entry: " + oldestDate).c_str());
            ImGui::SameLine(windowWidth - textSize.x);
            ImGui::Text("Oldest Entry: %s", oldestDate.c_str());

            std::lock_guard<std::mutex> lock(g_entriesMutex);
            auto& displayEntries = g_filteredEntries;

            ImGui::BeginChild("TableScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            if (ImGui::BeginTable("USNTable", 4,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable))
            {
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Date");
                ImGui::TableSetupColumn("Reason");
                ImGui::TableSetupColumn("Directory");
                ImGui::TableHeadersRow();

                if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
                {
                    if (sortSpecs->SpecsDirty)
                    {
                        const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                        std::sort(displayEntries.begin(), displayEntries.end(),
                            [&](const USNEntryRender& a, const USNEntryRender& b) {
                                switch (spec.ColumnIndex)
                                {
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

                if (g_resetScroll)
                {
                    ImGui::SetScrollY(0.0f);
                    g_resetScroll = false;
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(displayEntries.size()));
                while (clipper.Step())
                {
                    for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
                    {
                        const auto& e = displayEntries[row_n];
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.name.c_str());
                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(e.date.c_str());
                        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e.reason.c_str());
                        ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(e.directory.c_str());

                        std::string rowPopupName = "row_context_" + std::to_string(row_n);
                        std::string aggPopupName = "agg_events_popup_" + std::to_string(row_n);

                        ImGui::PushID(row_n);

                        static int g_modalOpenRow = -1;

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Selectable("##row_full", false, ImGuiSelectableFlags_SpanAllColumns);

                        if (ImGui::BeginPopupContextItem(("row_options_" + std::to_string(row_n)).c_str()))
                        {
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
                                [fileId](const USNEntryRender& r) { return r.fileId == fileId; });
                            if (it != g_entriesGrouped.end()) {
                                auto& grouped = *it;

                                ImGui::OpenPopup(("Entry Information - " + grouped.name).c_str());

                                ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
                                ImGui::SetNextWindowSizeConstraints(ImVec2(1000, 700), ImVec2(FLT_MAX, FLT_MAX));
                                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
                                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

                                if (ImGui::BeginPopupModal(("Entry Information - " + grouped.name).c_str(), NULL,
                                    ImGuiWindowFlags_None))
                                {
                                    float windowWidth = ImGui::GetWindowWidth();
                                    float buttonSize = ImGui::GetFrameHeight();
                                    float spacing = ImGui::GetStyle().ItemSpacing.x;

                                    ImGui::TextUnformatted("Name:");
                                    ImGui::SameLine();
                                    ImGui::TextUnformatted(grouped.name.c_str());

                                    ImGui::SameLine();
                                    ImGui::SetCursorPosX(windowWidth - buttonSize - spacing);

                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    if (ImGui::Button("X", ImVec2(buttonSize, buttonSize))) {
                                        ImGui::CloseCurrentPopup();
                                        g_modalOpenRow = -1;
                                    }
                                    ImGui::PopStyleVar();

                                    if (!grouped.events.empty()) {
                                        const auto& ev = grouped.events.back();

                                        ImGui::TextUnformatted("Date:");
                                        ImGui::SameLine();
                                        ImGui::TextUnformatted(ev.date.c_str());

                                        ImGui::TextUnformatted("Reason:");
                                        ImGui::SameLine();
                                        ImGui::TextUnformatted(ev.reason.c_str());

                                        ImGui::TextUnformatted("Directory:");
                                        ImGui::SameLine();
                                        ImGui::TextUnformatted(ev.directory.c_str());
                                    }
                                    if (ImGui::BeginTable("agg_events_table", 4,
                                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
                                    {
                                        ImGui::TableSetupColumn("Name");
                                        ImGui::TableSetupColumn("Date");
                                        ImGui::TableSetupColumn("Reason");
                                        ImGui::TableSetupColumn("Directory");
                                        ImGui::TableHeadersRow();

                                        for (auto it = grouped.events.rbegin(); it != grouped.events.rend(); ++it) {
                                            const auto& ev = *it;

                                            ImGui::TableNextRow();

                                            ImGui::TableSetColumnIndex(0);
                                            ImGui::TextUnformatted(ev.name.c_str());

                                            ImGui::TableSetColumnIndex(1);
                                            ImGui::TextUnformatted(ev.date.c_str());

                                            ImGui::TableSetColumnIndex(2);
                                            ImGui::TextUnformatted(ev.reason.c_str());

                                            ImGui::TableSetColumnIndex(3);
                                            ImGui::TextUnformatted(ev.directory.c_str());
                                        }
                                        ImGui::EndTable();
                                    }

                                    ImGui::EndPopup();
                                }

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
        }

        ImGui::End();
        ImGui::Render();

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
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