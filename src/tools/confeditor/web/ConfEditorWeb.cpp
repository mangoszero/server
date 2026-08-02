/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// Windows host for the HTML conf editor. WebView2 draws the UI; ConfModel owns
// the file. JS never touches the disk -- every open/save is a web message.

#include "ConfModel.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wrl.h>
#include <WebView2.h>

#include <string>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

#ifndef MANGOS_CLIENT_NAME
#define MANGOS_CLIENT_NAME "unknown client"
#endif

namespace
{
    conf::ConfFile g_conf;
    std::wstring   g_savePath;
    std::wstring   g_uiDir;
    bool           g_loaded = false;

    HWND g_main = nullptr;
    ComPtr<ICoreWebView2Controller> g_controller;
    ComPtr<ICoreWebView2>           g_webview;

    std::wstring Widen(const std::string& s)
    {
        if (s.empty())
        {
            return std::wstring();
        }
        const int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out((size_t)n, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], n);
        return out;
    }

    std::string Narrow(const std::wstring& s)
    {
        if (s.empty())
        {
            return std::string();
        }
        const int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(),
                                          nullptr, 0, nullptr, nullptr);
        std::string out((size_t)n, '\0');
        WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], n,
                            nullptr, nullptr);
        return out;
    }

    std::string JsonEscape(const std::string& s)
    {
        std::string o;
        o.reserve(s.size() + 8);
        for (unsigned char c : s)
        {
            switch (c)
            {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\b': o += "\\b"; break;
                case '\f': o += "\\f"; break;
                case '\n': o += "\\n"; break;
                case '\r': o += "\\r"; break;
                case '\t': o += "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        wsprintfA(buf, "\\u%04x", c);
                        o += buf;
                    }
                    else
                    {
                        o += (char)c;
                    }
                    break;
            }
        }
        return o;
    }

    const char* KindName(conf::Kind k)
    {
        switch (k)
        {
            case conf::KIND_NUMBER:     return "number";
            case conf::KIND_BOOL:       return "bool";
            case conf::KIND_CHOICE:     return "choice";
            case conf::KIND_ENUM:       return "enum";
            case conf::KIND_PATH:       return "path";
            case conf::KIND_CONNECTION: return "connection";
            default:                    return "text";
        }
    }

    bool Exists(const std::wstring& path)
    {
        const DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool EndsWith(const std::wstring& s, const wchar_t* tail)
    {
        const size_t n = wcslen(tail);
        return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
    }

    std::wstring ExeDirectory()
    {
        wchar_t buf[MAX_PATH] = { 0 };
        const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (!n || n >= MAX_PATH)
        {
            return std::wstring();
        }
        std::wstring path = buf;
        const size_t cut = path.find_last_of(L"\\/");
        return (cut == std::wstring::npos) ? std::wstring() : path.substr(0, cut);
    }

    std::wstring FileName(const std::wstring& path)
    {
        const size_t cut = path.find_last_of(L"\\/");
        return (cut == std::wstring::npos) ? path : path.substr(cut + 1);
    }

    std::wstring SaveTargetFor(const std::wstring& path)
    {
        if (EndsWith(path, L".conf.dist.in"))
        {
            return path.substr(0, path.size() - 8);
        }
        if (EndsWith(path, L".conf.dist"))
        {
            return path.substr(0, path.size() - 5);
        }
        return path;
    }

    std::wstring FindConf(const std::wstring& folder)
    {
        if (folder.empty())
        {
            return std::wstring();
        }

        static const wchar_t* const NAMES[] =
        {
            L"mangosd.conf", L"mangosd.conf.dist", L"mangosd.conf.dist.in",
            L"realmd.conf",  L"realmd.conf.dist",  L"realmd.conf.dist.in"
        };
        for (const wchar_t* n : NAMES)
        {
            const std::wstring candidate = folder + L"\\" + n;
            if (Exists(candidate))
            {
                return candidate;
            }
        }

        std::wstring best;
        int bestRank = 100;
        static const wchar_t* const PATS[] =
        {
            L"*.conf", L"*.conf.dist", L"*.conf.dist.in"
        };
        for (const wchar_t* pat : PATS)
        {
            WIN32_FIND_DATAW fd{};
            const HANDLE h = FindFirstFileW((folder + L"\\" + pat).c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE)
            {
                continue;
            }
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    continue;
                }
                int tier = 9;
                const std::wstring name = fd.cFileName;
                if (EndsWith(name, L".conf.dist.in"))
                {
                    tier = 2;
                }
                else if (EndsWith(name, L".conf.dist"))
                {
                    tier = 1;
                }
                else if (EndsWith(name, L".conf"))
                {
                    tier = 0;
                }
                std::wstring lower = name;
                CharLowerBuffW(&lower[0], (DWORD)lower.size());
                int who = 2;
                if (lower.find(L"mangosd") != std::wstring::npos)
                {
                    who = 0;
                }
                else if (lower.find(L"realmd") != std::wstring::npos)
                {
                    who = 1;
                }
                const int rank = tier * 10 + who;
                if (rank < bestRank)
                {
                    bestRank = rank;
                    best = folder + L"\\" + name;
                }
            }
            while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        return best;
    }

    /// Walk up from the exe: tools/confeditor-web → tools → install root.
    std::wstring FindConfBesideExe()
    {
        std::wstring dir = ExeDirectory();
        for (int up = 0; up < 4 && !dir.empty(); ++up)
        {
            const std::wstring found = FindConf(dir);
            if (!found.empty())
            {
                return found;
            }
            const size_t cut = dir.find_last_of(L"\\/");
            if (cut == std::wstring::npos)
            {
                break;
            }
            dir = dir.substr(0, cut);
        }
        return std::wstring();
    }

    std::wstring ResolveUiDir()
    {
        const std::wstring exe = ExeDirectory();
        static const wchar_t* const CAND[] =
        {
            L"\\confeditor-ui",
            L"\\ui",
            L"\\..\\..\\src\\tools\\confeditor\\web\\ui",
            L"\\..\\..\\..\\src\\tools\\confeditor\\web\\ui"
        };
        for (const wchar_t* rel : CAND)
        {
            const std::wstring dir = exe + rel;
            if (Exists(dir + L"\\index.html"))
            {
                wchar_t full[MAX_PATH] = { 0 };
                if (GetFullPathNameW(dir.c_str(), MAX_PATH, full, nullptr))
                {
                    return full;
                }
                return dir;
            }
        }
        return exe + L"\\confeditor-ui";
    }

    bool PickFolder(std::wstring& out)
    {
        BROWSEINFOW info{};
        wchar_t display[MAX_PATH] = { 0 };
        info.hwndOwner = g_main;
        info.pszDisplayName = display;
        info.lpszTitle = L"Pick the folder holding mangosd.conf";
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

        bool picked = false;
        if (LPITEMIDLIST idl = SHBrowseForFolderW(&info))
        {
            wchar_t chosen[MAX_PATH] = { 0 };
            if (SHGetPathFromIDListW(idl, chosen))
            {
                out = chosen;
                picked = true;
            }
            CoTaskMemFree(idl);
        }
        return picked;
    }

    void PostJson(const std::string& json)
    {
        if (!g_webview)
        {
            return;
        }
        g_webview->PostWebMessageAsJson(Widen(json).c_str());
    }

    void PostStatus(const std::string& text)
    {
        PostJson(std::string("{\"cmd\":\"status\",\"text\":\"") + JsonEscape(text) + "\"}");
    }

    void PostError(const std::string& text)
    {
        PostJson(std::string("{\"cmd\":\"error\",\"text\":\"") + JsonEscape(text) + "\"}");
    }

    void UpdateTitle()
    {
        std::wstring title = L"MaNGOS Config Editor (Web)";
        if (g_loaded)
        {
            const std::wstring loaded = Widen(g_conf.Path());
            title += L" \x2014 " + FileName(loaded);
            if (g_savePath != loaded)
            {
                title += L" \x2192 " + FileName(g_savePath);
            }
            if (g_conf.Dirty())
            {
                title += L" *";
            }
        }
        SetWindowTextW(g_main, title.c_str());
    }

    std::string BuildLoadedMessage()
    {
        std::string j = "{\"cmd\":\"loaded\"";
        j += ",\"path\":\"" + JsonEscape(g_conf.Path()) + "\"";
        j += ",\"savePath\":\"" + JsonEscape(Narrow(g_savePath)) + "\"";
        j += ",\"client\":\"" + JsonEscape(MANGOS_CLIENT_NAME) + "\"";
        j += ",\"dirty\":" + std::string(g_conf.Dirty() ? "true" : "false");

        j += ",\"groups\":[";
        const auto& groups = g_conf.Groups();
        for (size_t i = 0; i < groups.size(); ++i)
        {
            if (i)
            {
                j += ',';
            }
            j += "\"" + JsonEscape(groups[i]) + "\"";
        }
        j += "]";

        j += ",\"sections\":[";
        const auto& sections = g_conf.Sections();
        for (size_t i = 0; i < sections.size(); ++i)
        {
            if (i)
            {
                j += ',';
            }
            j += "{\"title\":\"" + JsonEscape(sections[i].title) + "\",\"group\":" +
                 std::to_string(sections[i].group) + "}";
        }
        j += "]";

        int placeholders = 0;
        j += ",\"entries\":[";
        const auto& entries = g_conf.Entries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const conf::Entry& e = entries[i];
            if (i)
            {
                j += ',';
            }
            if (e.value.size() > 2 && e.value.front() == '@' && e.value.back() == '@')
            {
                ++placeholders;
            }

            j += "{";
            j += "\"i\":" + std::to_string(i);
            j += ",\"key\":\"" + JsonEscape(e.key) + "\"";
            j += ",\"value\":\"" + JsonEscape(e.value) + "\"";
            j += ",\"doc\":\"" + JsonEscape(e.doc) + "\"";
            j += ",\"label\":\"" + JsonEscape(conf::Humanize(e.key)) + "\"";
            j += ",\"kind\":\"" + std::string(KindName(e.kind)) + "\"";
            j += ",\"quoted\":" + std::string(e.quoted ? "true" : "false");
            j += ",\"section\":" + std::to_string(e.section);
            j += ",\"group\":" + std::to_string(e.group);

            j += ",\"choices\":[";
            for (size_t c = 0; c < e.choices.size(); ++c)
            {
                if (c)
                {
                    j += ',';
                }
                j += "\"" + JsonEscape(e.choices[c]) + "\"";
            }
            j += "]";

            j += ",\"optionValues\":[";
            for (size_t c = 0; c < e.optionValues.size(); ++c)
            {
                if (c)
                {
                    j += ',';
                }
                j += "\"" + JsonEscape(e.optionValues[c]) + "\"";
            }
            j += "]";

            j += ",\"optionLabels\":[";
            for (size_t c = 0; c < e.optionLabels.size(); ++c)
            {
                if (c)
                {
                    j += ',';
                }
                j += "\"" + JsonEscape(e.optionLabels[c]) + "\"";
            }
            j += "]";

            j += "}";
        }
        j += "]";
        j += ",\"placeholders\":" + std::to_string(placeholders);
        j += "}";
        return j;
    }

    void OpenPath(const std::wstring& path)
    {
        std::string error;
        if (!g_conf.Load(Narrow(path), error))
        {
            PostError(error);
            return;
        }
        g_loaded = true;
        g_savePath = SaveTargetFor(path);
        UpdateTitle();
        PostJson(BuildLoadedMessage());
    }

    void DoSave(const std::wstring& path)
    {
        std::string error;
        if (!g_conf.Save(Narrow(path), error))
        {
            PostError(error);
            return;
        }
        g_savePath = path;
        g_conf.ClearDirty();
        UpdateTitle();
        PostStatus("Saved " + Narrow(path));
        PostJson("{\"cmd\":\"saved\",\"path\":\"" + JsonEscape(Narrow(path)) + "\"}");
    }

    // Minimal extractors for the tiny command language JS posts.
    bool JsonGetString(const std::string& json, const char* key, std::string& out)
    {
        const std::string needle = std::string("\"") + key + "\":\"";
        size_t at = json.find(needle);
        if (at == std::string::npos)
        {
            return false;
        }
        at += needle.size();
        std::string val;
        for (size_t i = at; i < json.size(); ++i)
        {
            if (json[i] == '\\' && i + 1 < json.size())
            {
                const char n = json[i + 1];
                if (n == '"' || n == '\\' || n == '/')
                {
                    val += n;
                }
                else if (n == 'n')
                {
                    val += '\n';
                }
                else if (n == 'r')
                {
                    val += '\r';
                }
                else if (n == 't')
                {
                    val += '\t';
                }
                else
                {
                    val += n;
                }
                ++i;
                continue;
            }
            if (json[i] == '"')
            {
                out = val;
                return true;
            }
            val += json[i];
        }
        return false;
    }

    void ApplyValuesFromMessage(const std::string& json)
    {
        // "values":[{"i":0,"v":"..."},...]
        size_t at = json.find("\"values\"");
        if (at == std::string::npos)
        {
            return;
        }
        at = json.find('[', at);
        if (at == std::string::npos)
        {
            return;
        }
        ++at;
        while (at < json.size())
        {
            while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                        json[at] == '\r' || json[at] == ','))
            {
                ++at;
            }
            if (at >= json.size() || json[at] == ']')
            {
                break;
            }
            if (json[at] != '{')
            {
                ++at;
                continue;
            }
            const size_t end = json.find('}', at);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string obj = json.substr(at, end - at + 1);

            size_t iPos = obj.find("\"i\":");
            size_t vPos = obj.find("\"v\":\"");
            if (iPos != std::string::npos && vPos != std::string::npos)
            {
                const int idx = atoi(obj.c_str() + iPos + 4);
                std::string val;
                if (JsonGetString(obj, "v", val) && idx >= 0 &&
                    (size_t)idx < g_conf.Entries().size())
                {
                    g_conf.SetValue((size_t)idx, val);
                }
            }
            at = end + 1;
        }
        UpdateTitle();
    }

    void OnWebMessage(const std::string& json)
    {
        std::string cmd;
        if (!JsonGetString(json, "cmd", cmd))
        {
            return;
        }

        if (cmd == "ready")
        {
            if (g_loaded)
            {
                UpdateTitle();
                PostJson(BuildLoadedMessage());
                return;
            }
            const std::wstring found = FindConfBesideExe();
            if (!found.empty())
            {
                OpenPath(found);
            }
            else
            {
                PostStatus(std::string("Open a folder holding mangosd.conf — ") +
                           MANGOS_CLIENT_NAME);
            }
            return;
        }

        if (cmd == "open")
        {
            std::wstring folder;
            if (!PickFolder(folder))
            {
                return;
            }
            const std::wstring found = FindConf(folder);
            if (found.empty())
            {
                PostError("No mangosd.conf / .dist / realmd.conf in that folder.");
                return;
            }
            OpenPath(found);
            return;
        }

        if (cmd == "reload")
        {
            if (!g_loaded)
            {
                return;
            }
            ApplyValuesFromMessage(json);
            std::wstring path = g_savePath;
            if (!Exists(path))
            {
                path = Widen(g_conf.Path());
            }
            if (!Exists(path))
            {
                path = FindConfBesideExe();
            }
            if (path.empty())
            {
                PostError("Nothing on disk to reload.");
                return;
            }
            OpenPath(path);
            return;
        }

        if (cmd == "save" || cmd == "saveas")
        {
            if (!g_loaded)
            {
                return;
            }
            ApplyValuesFromMessage(json);

            std::wstring path = g_savePath;
            if (cmd == "saveas")
            {
                wchar_t chosen[MAX_PATH] = { 0 };
                lstrcpynW(chosen, path.c_str(), MAX_PATH);
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = g_main;
                ofn.lpstrFilter = L"Config files\0*.conf;*.dist;*.in\0All files\0*.*\0\0";
                ofn.lpstrFile = chosen;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrTitle = L"Save configuration as";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
                if (!GetSaveFileNameW(&ofn))
                {
                    return;
                }
                path = chosen;
            }
            DoSave(path);
            return;
        }

        if (cmd == "dirty")
        {
            ApplyValuesFromMessage(json);
            return;
        }
    }

    void ResizeWebView()
    {
        if (!g_controller)
        {
            return;
        }
        RECT rc{};
        GetClientRect(g_main, &rc);
        g_controller->put_Bounds(rc);
    }

    void InitWebView()
    {
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
                {
                    if (FAILED(result) || !env)
                    {
                        MessageBoxW(g_main,
                                    L"WebView2 Runtime is missing.\n"
                                    L"Install the Evergreen runtime from Microsoft.",
                                    L"MaNGOS Config Editor (Web)",
                                    MB_OK | MB_ICONERROR);
                        return result;
                    }

                    env->CreateCoreWebView2Controller(
                        g_main,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                            {
                                if (FAILED(result) || !controller)
                                {
                                    return result;
                                }

                                g_controller = controller;
                                g_controller->get_CoreWebView2(&g_webview);
                                ResizeWebView();

                                ComPtr<ICoreWebView2Settings> settings;
                                g_webview->get_Settings(&settings);
                                if (settings)
                                {
                                    settings->put_IsStatusBarEnabled(FALSE);
                                    settings->put_AreDefaultContextMenusEnabled(TRUE);
                                    settings->put_IsZoomControlEnabled(TRUE);
                                }

                                EventRegistrationToken token{};
                                g_webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [](ICoreWebView2*,
                                           ICoreWebView2WebMessageReceivedEventArgs* args)
                                        -> HRESULT
                                        {
                                            LPWSTR raw = nullptr;
                                            if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) &&
                                                raw)
                                            {
                                                OnWebMessage(Narrow(raw));
                                                CoTaskMemFree(raw);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &token);

                                // Prefer virtual host so relative assets work offline.
                                ComPtr<ICoreWebView2_3> wv3;
                                if (SUCCEEDED(g_webview.As(&wv3)) && !g_uiDir.empty())
                                {
                                    wv3->SetVirtualHostNameToFolderMapping(
                                        L"app.local", g_uiDir.c_str(),
                                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                    g_webview->Navigate(L"https://app.local/index.html");
                                }
                                else
                                {
                                    std::wstring fileUrl = L"file:///";
                                    for (wchar_t c : g_uiDir)
                                    {
                                        fileUrl += (c == L'\\') ? L'/' : c;
                                    }
                                    fileUrl += L"/index.html";
                                    g_webview->Navigate(fileUrl.c_str());
                                }
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());

        if (FAILED(hr))
        {
            MessageBoxW(g_main, L"Could not start WebView2 environment.",
                        L"MaNGOS Config Editor (Web)", MB_OK | MB_ICONERROR);
        }
    }

    LRESULT CALLBACK MainProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
            case WM_SIZE:
                if (wp != SIZE_MINIMIZED)
                {
                    ResizeWebView();
                }
                return 0;

            case WM_DESTROY:
                if (g_controller)
                {
                    g_controller->Close();
                    g_controller = nullptr;
                }
                g_webview = nullptr;
                PostQuitMessage(0);
                return 0;

            default:
                break;
        }
        return DefWindowProcW(w, msg, wp, lp);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int show)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    g_uiDir = ResolveUiDir();
    if (!Exists(g_uiDir + L"\\index.html"))
    {
        MessageBoxW(nullptr,
                    (L"UI not found.\nExpected index.html under:\n" + g_uiDir).c_str(),
                    L"MaNGOS Config Editor (Web)", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MangosConfEditorWeb";
    RegisterClassExW(&wc);

    RECT work{ 0, 0, 1100, 720 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int width = (work.right - work.left) * 9 / 10;
    const int height = (work.bottom - work.top) * 9 / 10;
    const int x = work.left + ((work.right - work.left) - width) / 2;
    const int y = work.top + ((work.bottom - work.top) - height) / 2;

    g_main = CreateWindowExW(0, L"MangosConfEditorWeb", L"MaNGOS Config Editor (Web)",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             x, y, width, height,
                             nullptr, nullptr, instance, nullptr);
    if (!g_main)
    {
        CoUninitialize();
        return 1;
    }

    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    InitWebView();

    // Optional path on the command line.
    if (commandLine && *commandLine)
    {
        std::wstring arg = commandLine;
        const size_t first = arg.find_first_not_of(L" \t\"");
        const size_t last = arg.find_last_not_of(L" \t\"");
        arg = (first == std::wstring::npos) ? std::wstring()
                                            : arg.substr(first, last - first + 1);
        // Defer until webview ready -- store and open on ready if needed.
        // For simplicity, if the webview is not up yet, ready handler still
        // auto-finds; command line open is done after a short navigate via
        // posting once loaded. Open immediately into model; UI pulls on ready.
        const DWORD attributes = GetFileAttributesW(arg.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES)
        {
            const std::wstring path =
                (attributes & FILE_ATTRIBUTE_DIRECTORY) ? FindConf(arg) : arg;
            if (!path.empty())
            {
                // Load now; ready will re-send. Mark by opening after ready only:
                // simplest: set g_savePath and load when ready if already loaded.
                std::string error;
                if (g_conf.Load(Narrow(path), error))
                {
                    g_loaded = true;
                    g_savePath = SaveTargetFor(path);
                }
            }
        }
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
