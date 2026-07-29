/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// A window in front of mangos-extractor, for people who do not live in a terminal.
//
// IT DRIVES THE TOOL THROUGH ITS COMMAND LINE and reads its stdout -- it does not link
// the baker, include its headers or know a thing about MPQs. The command line IS the
// extractor's public interface; going around it would couple a dialog to the internals
// of a baker, and the two would then have to move together forever.
//
// So this file is allowed to be Windows-only and ugly. It builds its controls in code
// rather than from a resource script, because one .cpp with no .rc is one fewer thing
// that can disagree with itself.

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <string>
#include <vector>

// Visual styles. Without this the process binds to comctl32 version 5 and every
// control keeps its 1998 look, whatever InitCommonControlsEx says.
#if defined(_MSC_VER)
#pragma comment(linker, "/manifestdependency:\"type='win32' "                       \
                        "name='Microsoft.Windows.Common-Controls' "                  \
                        "version='6.0.0.0' processorArchitecture='*' "               \
                        "publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


namespace
{
    enum : int
    {
        ID_SRC = 1001, ID_SRC_BROWSE,
        ID_DEST, ID_DEST_BROWSE,
        ID_VESSELS, ID_VESSELS_BROWSE,
        ID_OFFMESH, ID_OFFMESH_BROWSE,
        ID_MAP,
        ID_CHK_DBC, ID_CHK_GOMODELS, ID_CHK_TILES, ID_CHK_TRANS, ID_CHK_NAV,
        ID_ALL, ID_NONE,
        ID_START, ID_CLOSE,
        ID_LOG, ID_PROGRESS, ID_STATUS
    };

    const UINT WM_APP_LINE = WM_APP + 1;   ///< wParam: heap-allocated std::string*
    const UINT WM_APP_DONE = WM_APP + 2;   ///< wParam: child exit code

    HWND g_main = nullptr;
    HWND g_log = nullptr;
    HWND g_progress = nullptr;
    HWND g_status = nullptr;
    HANDLE g_worker = nullptr;
    volatile LONG g_running = 0;

    struct Control
    {
        int id;
        HWND hwnd;
    };
    std::vector<Control> g_controls;

    HWND Find(int id)
    {
        for (const Control& c : g_controls)
        {
            if (c.id == id)
            {
                return c.hwnd;
            }
        }
        return nullptr;
    }

    /// The shell's own dialog font. DEFAULT_GUI_FONT is MS Sans Serif and looks it.
    HFONT UiFont()
    {
        static HFONT font = nullptr;
        if (!font)
        {
            NONCLIENTMETRICSA ncm{};
            ncm.cbSize = sizeof(ncm);
            if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            {
                font = CreateFontIndirectA(&ncm.lfMessageFont);
            }
            if (!font)
            {
                font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }
        }
        return font;
    }

    HWND Add(HWND parent, const char* cls, const char* text, DWORD style,
             int x, int y, int w, int h, int id)
    {
        HWND h2 = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                  x, y, w, h, parent, (HMENU)(INT_PTR)id,
                                  GetModuleHandleA(nullptr), nullptr);
        SendMessageA(h2, WM_SETFONT, (WPARAM)UiFont(), TRUE);
        if (id)
        {
            g_controls.push_back(Control{id, h2});
        }
        return h2;
    }

    std::string GetText(int id)
    {
        HWND h = Find(id);
        if (!h)
        {
            return std::string();
        }
        const int n = GetWindowTextLengthA(h);
        std::string s(size_t(n) + 1, '\0');
        GetWindowTextA(h, &s[0], n + 1);
        s.resize(size_t(n));
        return s;
    }

    bool Checked(int id)
    {
        HWND h = Find(id);
        return h && SendMessageA(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    void Check(int id, bool on)
    {
        if (HWND h = Find(id))
        {
            SendMessageA(h, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    void AppendLog(const std::string& line)
    {
        const int end = GetWindowTextLengthA(g_log);
        SendMessageA(g_log, EM_SETSEL, (WPARAM)end, (LPARAM)end);
        std::string text = line + "\r\n";
        SendMessageA(g_log, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    }

    bool PickFolder(HWND owner, const char* title, std::string& out)
    {
        BROWSEINFOA info{};
        char display[MAX_PATH] = {0};
        info.hwndOwner = owner;
        info.pszDisplayName = display;
        info.lpszTitle = title;
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

        bool picked = false;
        if (LPITEMIDLIST idl = SHBrowseForFolderA(&info))
        {
            char chosen[MAX_PATH] = {0};
            if (SHGetPathFromIDListA(idl, chosen))
            {
                out = chosen;
                picked = true;
            }
            CoTaskMemFree(idl);
        }
        return picked;
    }

    bool PickFile(HWND owner, const char* title, std::string& out)
    {
        char chosen[MAX_PATH] = {0};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = "Text files\0*.txt\0All files\0*.*\0\0";
        ofn.lpstrFile = chosen;
        ofn.nMaxFile = sizeof(chosen);
        ofn.lpstrTitle = title;
        // NOCHANGEDIR: the child resolves its own defaults against the working
        // directory, so a dialog must not move it out from under them.
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameA(&ofn))
        {
            return false;
        }
        out = chosen;
        return true;
    }

    std::string ExeDir()
    {
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string s = path;
        const size_t cut = s.find_last_of("\\/");
        return (cut == std::string::npos) ? std::string(".") : s.substr(0, cut);
    }

    std::string Quote(const std::string& s)
    {
        return "\"" + s + "\"";
    }

    /// The command line, exactly as a person would have typed it.
    std::string BuildCommand()
    {
        std::string cmd = Quote(ExeDir() + "\\mangos-extractor.exe");

        if (Checked(ID_CHK_DBC) && Checked(ID_CHK_GOMODELS) && Checked(ID_CHK_TILES) &&
            Checked(ID_CHK_TRANS) && Checked(ID_CHK_NAV))
        {
            cmd += " all";
        }
        else
        {
            if (Checked(ID_CHK_DBC))      { cmd += " dbc"; }
            if (Checked(ID_CHK_GOMODELS)) { cmd += " gomodels"; }
            if (Checked(ID_CHK_TILES))    { cmd += " tile"; }
            if (Checked(ID_CHK_TRANS))    { cmd += " trans"; }
            if (Checked(ID_CHK_NAV))      { cmd += " nav"; }
        }

        const std::string src = GetText(ID_SRC);
        const std::string dest = GetText(ID_DEST);
        const std::string vessels = GetText(ID_VESSELS);
        const std::string offmesh = GetText(ID_OFFMESH);
        const std::string map = GetText(ID_MAP);

        if (!src.empty())     { cmd += " --src " + Quote(src); }
        if (!dest.empty())    { cmd += " --dest " + Quote(dest); }
        if (!vessels.empty()) { cmd += " --vessels " + Quote(vessels); }
        if (!offmesh.empty()) { cmd += " --offmesh " + Quote(offmesh); }
        if (!map.empty())     { cmd += " --map " + map; }

        cmd += " --no-menu";
        return cmd;
    }

    struct Job
    {
        std::string command;
    };

    DWORD WINAPI RunChild(LPVOID param)
    {
        Job* job = static_cast<Job*>(param);

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        DWORD exitCode = DWORD(-1);

        if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
        {
            PostMessageA(g_main, WM_APP_DONE, WPARAM(exitCode), 0);
            delete job;
            return 0;
        }
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = writeEnd;
        si.hStdError = writeEnd;
        si.hStdInput = nullptr;

        PROCESS_INFORMATION pi{};
        std::vector<char> mutableCmd(job->command.begin(), job->command.end());
        mutableCmd.push_back('\0');

        const BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr,
                                       TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                       &si, &pi);
        CloseHandle(writeEnd);

        if (!ok)
        {
            PostMessageA(g_main, WM_APP_LINE,
                         WPARAM(new std::string("could not start mangos-extractor.exe -- "
                                                "it must sit beside this program")), 0);
            CloseHandle(readEnd);
            PostMessageA(g_main, WM_APP_DONE, WPARAM(exitCode), 0);
            delete job;
            return 0;
        }

        std::string pending;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(readEnd, buf, sizeof(buf), &got, nullptr) && got)
        {
            pending.append(buf, got);
            size_t nl;
            while ((nl = pending.find('\n')) != std::string::npos)
            {
                std::string line = pending.substr(0, nl);
                pending.erase(0, nl + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == '\0'))
                {
                    line.pop_back();
                }
                PostMessageA(g_main, WM_APP_LINE, WPARAM(new std::string(line)), 0);
            }
        }
        if (!pending.empty())
        {
            PostMessageA(g_main, WM_APP_LINE, WPARAM(new std::string(pending)), 0);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(readEnd);

        PostMessageA(g_main, WM_APP_DONE, WPARAM(exitCode), 0);
        delete job;
        return 0;
    }

    void SetBusy(bool busy)
    {
        static const int gated[] = {
            ID_START, ID_SRC_BROWSE, ID_DEST_BROWSE, ID_VESSELS_BROWSE,
            ID_OFFMESH_BROWSE, ID_ALL, ID_NONE,
            ID_CHK_DBC, ID_CHK_GOMODELS, ID_CHK_TILES, ID_CHK_TRANS, ID_CHK_NAV
        };
        for (int id : gated)
        {
            if (HWND h = Find(id))
            {
                EnableWindow(h, !busy);
            }
        }
        // Stopping a marquee leaves its last block painted, so the bar is hidden
        // outright rather than merely stilled -- and the style is cleared with it,
        // because a marquee bar ignores PBM_SETPOS and would bring that block back.
        if (busy)
        {
            const LONG_PTR style = GetWindowLongPtrA(g_progress, GWL_STYLE);
            SetWindowLongPtrA(g_progress, GWL_STYLE, style | LONG_PTR(PBS_MARQUEE));
            SendMessageA(g_progress, PBM_SETMARQUEE, TRUE, 30);
        }
        else
        {
            SendMessageA(g_progress, PBM_SETMARQUEE, FALSE, 0);
            const LONG_PTR style = GetWindowLongPtrA(g_progress, GWL_STYLE);
            SetWindowLongPtrA(g_progress, GWL_STYLE, style & ~LONG_PTR(PBS_MARQUEE));
            SendMessageA(g_progress, PBM_SETRANGE32, 0, 100);
            SendMessageA(g_progress, PBM_SETPOS, 0, 0);
        }
        ShowWindow(g_progress, busy ? SW_SHOW : SW_HIDE);

        SetWindowTextA(g_status, busy ? "Baking -- the navmesh can take hours."
                                      : "Idle.");
    }

    void OnStart()
    {
        if (!Checked(ID_CHK_DBC) && !Checked(ID_CHK_GOMODELS) && !Checked(ID_CHK_TILES) &&
            !Checked(ID_CHK_TRANS) && !Checked(ID_CHK_NAV))
        {
            MessageBoxA(g_main, "Tick at least one thing to extract.",
                        "Nothing selected", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (Checked(ID_CHK_TRANS) && !Checked(ID_CHK_GOMODELS))
        {
            const int answer = MessageBoxA(g_main,
                "Transports are carved out of baked gomodels. If gomodels were not "
                "baked earlier, this run produces nothing.\n\nCarry on?",
                "Transports need gomodels", MB_YESNO | MB_ICONWARNING);
            if (answer != IDYES)
            {
                return;
            }
        }
        if (Checked(ID_CHK_NAV) && !Checked(ID_CHK_TILES))
        {
            const int answer = MessageBoxA(g_main,
                "The navmesh is built from baked tiles. If tiles were not baked "
                "earlier, this run produces nothing.\n\nCarry on?",
                "Navmesh needs tiles", MB_YESNO | MB_ICONWARNING);
            if (answer != IDYES)
            {
                return;
            }
        }

        Job* job = new Job{BuildCommand()};
        AppendLog("> " + job->command);

        InterlockedExchange(&g_running, 1);
        SetBusy(true);
        g_worker = CreateThread(nullptr, 0, RunChild, job, 0, nullptr);
        if (!g_worker)
        {
            delete job;
            InterlockedExchange(&g_running, 0);
            SetBusy(false);
        }
    }

    void BuildUi(HWND w)
    {
        const int labelW = 130;
        const int editX = labelW + 20;
        const int editW = 400;
        const int btnX = editX + editW + 10;
        const int btnW = 90;
        int y = 14;

        struct Row
        {
            const char* label;
            int editId;
            int buttonId;
            const char* buttonText;
        };
        const Row rows[] = {
            {"Client (Data folder)", ID_SRC,     ID_SRC_BROWSE,     "Browse..."},
            {"Output folder",        ID_DEST,    ID_DEST_BROWSE,    "Browse..."},
            {"vessels.txt",          ID_VESSELS, ID_VESSELS_BROWSE, "Choose..."},
            {"offmesh.txt",          ID_OFFMESH, ID_OFFMESH_BROWSE, "Choose..."}
        };

        for (const Row& r : rows)
        {
            Add(w, "STATIC", r.label, SS_LEFT, 14, y + 4, labelW, 20, 0);
            Add(w, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                editX, y, editW, 24, r.editId);
            Add(w, "BUTTON", r.buttonText, WS_TABSTOP | BS_PUSHBUTTON,
                btnX, y, btnW, 24, r.buttonId);
            y += 32;
        }

        Add(w, "STATIC", "Map id (blank = all)", SS_LEFT, 14, y + 4, labelW, 20, 0);
        Add(w, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_NUMBER, editX, y, 80, 24, ID_MAP);
        y += 40;

        Add(w, "BUTTON", "Extract", BS_GROUPBOX, 14, y, btnX + btnW - 14, 96, 0);
        const int cy = y + 22;
        Add(w, "BUTTON", "DBC / DB2", WS_TABSTOP | BS_AUTOCHECKBOX, 28, cy, 110, 22,
            ID_CHK_DBC);
        Add(w, "BUTTON", "GO models", WS_TABSTOP | BS_AUTOCHECKBOX, 148, cy, 110, 22,
            ID_CHK_GOMODELS);
        Add(w, "BUTTON", "Tiles (minutes)", WS_TABSTOP | BS_AUTOCHECKBOX, 268, cy, 120, 22,
            ID_CHK_TILES);
        Add(w, "BUTTON", "Transports", WS_TABSTOP | BS_AUTOCHECKBOX, 398, cy, 110, 22,
            ID_CHK_TRANS);
        Add(w, "BUTTON", "Navmesh (hours)", WS_TABSTOP | BS_AUTOCHECKBOX, 518, cy, 120, 22,
            ID_CHK_NAV);
        Add(w, "BUTTON", "Select all", WS_TABSTOP | BS_PUSHBUTTON, 28, cy + 30, 100, 24,
            ID_ALL);
        Add(w, "BUTTON", "Clear", WS_TABSTOP | BS_PUSHBUTTON, 138, cy + 30, 100, 24,
            ID_NONE);
        y += 108;

        g_progress = Add(w, PROGRESS_CLASSA, "", PBS_MARQUEE, 14, y,
                         btnX + btnW - 14, 18, ID_PROGRESS);
        y += 26;

        ShowWindow(g_progress, SW_HIDE);

        g_status = Add(w, "STATIC", "Idle.", SS_LEFT, 14, y, btnX + btnW - 14, 20,
                       ID_STATUS);
        y += 26;

        g_log = Add(w, "EDIT", "",
                    WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                    14, y, btnX + btnW - 14, 220, ID_LOG);
        y += 230;

        Add(w, "BUTTON", "Start", WS_TABSTOP | BS_DEFPUSHBUTTON,
            btnX + btnW - 200, y, 90, 28, ID_START);
        Add(w, "BUTTON", "Close", WS_TABSTOP | BS_PUSHBUTTON,
            btnX + btnW - 100, y, 90, 28, ID_CLOSE);

        Check(ID_CHK_DBC, true);
        Check(ID_CHK_GOMODELS, true);
        SetWindowTextA(Find(ID_SRC), (ExeDir() + "\\Data").c_str());
        SetWindowTextA(Find(ID_DEST), (ExeDir() + "\\extracted_data").c_str());
    }

    LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_CREATE:
            BuildUi(w);
            return 0;

        case WM_APP_LINE:
        {
            std::string* line = reinterpret_cast<std::string*>(wp);
            AppendLog(*line);
            delete line;
            return 0;
        }

        case WM_APP_DONE:
        {
            InterlockedExchange(&g_running, 0);
            SetBusy(false);
            if (g_worker)
            {
                CloseHandle(g_worker);
                g_worker = nullptr;
            }
            const DWORD code = DWORD(wp);
            AppendLog(code == 0 ? "-- finished --"
                                : "-- failed, exit code " + std::to_string(code) + " --");
            SetWindowTextA(g_status, code == 0 ? "Done." : "Failed -- see the log.");
            return 0;
        }

        case WM_COMMAND:
        {
            const int id = LOWORD(wp);
            std::string picked;
            switch (id)
            {
            case ID_SRC_BROWSE:
                if (PickFolder(w, "Choose the client's Data folder", picked))
                {
                    SetWindowTextA(Find(ID_SRC), picked.c_str());
                }
                return 0;
            case ID_DEST_BROWSE:
                if (PickFolder(w, "Choose the output folder", picked))
                {
                    SetWindowTextA(Find(ID_DEST), picked.c_str());
                }
                return 0;
            case ID_VESSELS_BROWSE:
                if (PickFile(w, "Choose vessels.txt", picked))
                {
                    SetWindowTextA(Find(ID_VESSELS), picked.c_str());
                }
                return 0;
            case ID_OFFMESH_BROWSE:
                if (PickFile(w, "Choose offmesh.txt", picked))
                {
                    SetWindowTextA(Find(ID_OFFMESH), picked.c_str());
                }
                return 0;
            case ID_ALL:
                Check(ID_CHK_DBC, true);
                Check(ID_CHK_GOMODELS, true);
                Check(ID_CHK_TILES, true);
                Check(ID_CHK_TRANS, true);
                Check(ID_CHK_NAV, true);
                return 0;
            case ID_NONE:
                Check(ID_CHK_DBC, false);
                Check(ID_CHK_GOMODELS, false);
                Check(ID_CHK_TILES, false);
                Check(ID_CHK_TRANS, false);
                Check(ID_CHK_NAV, false);
                return 0;
            case ID_START:
                OnStart();
                return 0;
            case ID_CLOSE:
                SendMessageA(w, WM_CLOSE, 0, 0);
                return 0;
            default:
                break;
            }
            return 0;
        }

        case WM_CLOSE:
            if (InterlockedCompareExchange(&g_running, 0, 0) != 0)
            {
                const int answer = MessageBoxA(w,
                    "A bake is still running. Closing now kills it and leaves the "
                    "output half written.\n\nClose anyway?",
                    "Still baking", MB_YESNO | MB_ICONWARNING);
                if (answer != IDYES)
                {
                    return 0;
                }
            }
            DestroyWindow(w);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcA(w, msg, wp, lp);
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show)
{
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MangosExtractorGui";
    if (!RegisterClassA(&wc))
    {
        return 1;
    }

    RECT wanted{0, 0, 680, 640};
    AdjustWindowRect(&wanted, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                     FALSE);

    g_main = CreateWindowExA(0, wc.lpszClassName, "MaNGOS client baker",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             wanted.right - wanted.left, wanted.bottom - wanted.top,
                             nullptr, nullptr, inst, nullptr);
    if (!g_main)
    {
        return 1;
    }

    ShowWindow(g_main, show);
    UpdateWindow(g_main);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageA(g_main, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return 0;
}
