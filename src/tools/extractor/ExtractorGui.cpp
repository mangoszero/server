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
#include <olectl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
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


#ifndef MANGOS_CLIENT_NAME
#define MANGOS_CLIENT_NAME "unknown client"
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
        ID_LOCALE, ID_CHK_ALLLOC,
        ID_CHK_SHUTDOWN, ID_TIMES,
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
    SYSTEMTIME g_startedAt{};
    ULONGLONG g_startedTick = 0;

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

    /// The languages a client actually carries, by the same test the baker uses: a folder
    /// is a locale only if it holds the archive named after it.
    std::vector<std::string> FindLocales(const std::string& dataDir)
    {
        std::vector<std::string> found;
        if (dataDir.empty())
        {
            return found;
        }

        WIN32_FIND_DATAA fd{};
        HANDLE h = FindFirstFileA((dataDir + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            return found;
        }
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.')
            {
                continue;
            }
            const std::string name = fd.cFileName;
            const std::string mpq = dataDir + "\\" + name + "\\locale-" + name + ".MPQ";
            if (GetFileAttributesA(mpq.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                found.push_back(name);
            }
        }
        while (FindNextFileA(h, &fd));
        FindClose(h);

        std::sort(found.begin(), found.end());
        return found;
    }

    /// Refill the language list from whatever the client box currently points at. Called
    /// on every edit of it, so browsing to another client re-reads rather than going stale.
    void RefreshLocales()
    {
        HWND box = Find(ID_LOCALE);
        if (!box)
        {
            return;
        }

        const std::string previous = GetText(ID_LOCALE);
        SendMessageA(box, CB_RESETCONTENT, 0, 0);
        SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)"(detect)");

        int pick = 0;
        int i = 1;
        for (const std::string& loc : FindLocales(GetText(ID_SRC)))
        {
            SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)loc.c_str());
            if (loc == previous)
            {
                pick = i;
            }
            ++i;
        }
        SendMessageA(box, CB_SETCURSEL, WPARAM(pick), 0);
    }

    /// snprintf, NOT wsprintf. wsprintf understands no 64-bit length modifier at all:
    /// given "%llum" it consumed the conversion and printed the literal tail, so a nine
    /// second run reported "Took 1um 1us". It has no way to report that it did not
    /// understand the format, which is why the output looked like a unit and not an error.
    std::string Clock(const SYSTEMTIME& t)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                      unsigned(t.wHour), unsigned(t.wMinute), unsigned(t.wSecond));
        return buf;
    }

    std::string Elapsed(ULONGLONG ms)
    {
        const unsigned s = unsigned(ms / 1000);
        char buf[32];
        if (s >= 3600)
        {
            std::snprintf(buf, sizeof(buf), "%uh %02um", s / 3600, (s % 3600) / 60);
        }
        else if (s >= 60)
        {
            std::snprintf(buf, sizeof(buf), "%um %02us", s / 60, s % 60);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%us", s);
        }
        return buf;
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

        // "all" is the extractor's own word for every language on the disc; a named
        // one pins it; "(detect)" sends nothing and lets the baker choose.
        const std::string locale = GetText(ID_LOCALE);
        if (Checked(ID_CHK_ALLLOC))
        {
            cmd += " --locale all";
        }
        else if (!locale.empty() && locale != "(detect)")
        {
            cmd += " --locale " + locale;
        }

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
            ID_OFFMESH_BROWSE, ID_ALL, ID_NONE, ID_LOCALE, ID_CHK_ALLLOC,
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


    /// Ask Windows to shut down, the way a scheduled task would: a delay long enough to
    /// abort by hand, and the privilege it needs, which a process does not hold by default.
    void ShutdownPc()
    {
        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(),
                             TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            TOKEN_PRIVILEGES tp{};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValueA(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid))
            {
                AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
            }
            CloseHandle(token);
        }

        InitiateSystemShutdownExA(nullptr,
                                  const_cast<char*>("The client bake has finished."),
                                  60, FALSE, FALSE,
                                  SHTDN_REASON_MAJOR_APPLICATION |
                                  SHTDN_REASON_MINOR_MAINTENANCE |
                                  SHTDN_REASON_FLAG_PLANNED);
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

        GetLocalTime(&g_startedAt);
        g_startedTick = GetTickCount64();
        SetWindowTextA(Find(ID_TIMES), ("Started " + Clock(g_startedAt)).c_str());

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


    /* ------------------------------------------------------------------ header badge */

    const int kBandH   = 92;    ///< the header band, above everything else
    const int kBadge   = 68;    ///< the logo, square, drawn inside it
    const int kMargin  = 14;

    IPicture* g_badge = nullptr;

    /// Decode the embedded JPEG once. A JPEG cannot be a BITMAP resource, so it ships as
    /// raw bytes and OleLoadPicture does the decoding -- no image library is linked.
    IPicture* Badge()
    {
        static bool tried = false;
        if (tried)
        {
            return g_badge;
        }
        tried = true;

        HMODULE self = GetModuleHandleA(nullptr);
        HRSRC found = FindResourceA(self, "MANGOSLOGO", RT_RCDATA);
        if (!found)
        {
            return nullptr;
        }

        const DWORD size = SizeofResource(self, found);
        HGLOBAL res = LoadResource(self, found);
        void* bytes = res ? LockResource(res) : nullptr;
        if (!bytes || !size)
        {
            return nullptr;
        }

        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!mem)
        {
            return nullptr;
        }
        if (void* dst = GlobalLock(mem))
        {
            memcpy(dst, bytes, size);
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (SUCCEEDED(CreateStreamOnHGlobal(mem, TRUE, &stream)) && stream)
            {
                OleLoadPicture(stream, LONG(size), FALSE, IID_IPicture,
                               reinterpret_cast<void**>(&g_badge));
                stream->Release();
                return g_badge;      // the stream owns `mem` now
            }
        }

        GlobalFree(mem);
        return nullptr;
    }

    void PaintHeader(HWND w, HDC dc, RECT const& client)
    {
        RECT band{0, 0, client.right, kBandH};

        // A band a shade off the dialog face, so the form below reads as the working area.
        HBRUSH back = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(dc, &band, back);
        DeleteObject(back);

        // The rule that closes it. One pixel, the 3D shadow colour: enough to separate,
        // not enough to draw attention.
        HPEN rule = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
        HGDIOBJ oldPen = SelectObject(dc, rule);
        MoveToEx(dc, 0, kBandH - 1, nullptr);
        LineTo(dc, client.right, kBandH - 1);
        SelectObject(dc, oldPen);
        DeleteObject(rule);

        const int by = (kBandH - kBadge) / 2;

        if (IPicture* pic = Badge())
        {
            OLE_XSIZE_HIMETRIC cx = 0;
            OLE_YSIZE_HIMETRIC cy = 0;
            pic->get_Width(&cx);
            pic->get_Height(&cy);

            // Keep it square-true whatever the source is: fit the longer side to the box.
            int dw = kBadge, dh = kBadge;
            if (cx > 0 && cy > 0)
            {
                if (cx >= cy)
                {
                    dh = int(double(kBadge) * double(cy) / double(cx));
                }
                else
                {
                    dw = int(double(kBadge) * double(cx) / double(cy));
                }
            }

            const int dx = kMargin + (kBadge - dw) / 2;
            const int dy = by + (kBadge - dh) / 2;

            const int mode = SetStretchBltMode(dc, HALFTONE);
            SetBrushOrgEx(dc, 0, 0, nullptr);
            pic->Render(dc, dx, dy, dw, dh, 0, cy, cx, -cy, nullptr);
            SetStretchBltMode(dc, mode);
        }

        const int tx = kMargin + kBadge + 16;

        LOGFONTA lf{};
        HFONT base = UiFont();
        GetObjectA(base, sizeof(lf), &lf);

        LOGFONTA big = lf;
        big.lfHeight = LONG(double(lf.lfHeight) * 1.7);
        big.lfWeight = FW_SEMIBOLD;
        HFONT title = CreateFontIndirectA(&big);

        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(dc, title);
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));

        // Measured, not counted by hand. The literals used to carry their own lengths as
        // magic numbers, so editing the text silently truncated it or ran off the end.
        const char* heading = "MaNGOS client baker";
        TextOutA(dc, tx, by + 6, heading, int(std::strlen(heading)));

        // WHICH CLIENT THIS BUILD BAKES. One baker cannot read two expansions -- the DBC
        // layouts and the archive set both differ -- so the version is not decoration, it
        // is the first thing that has to match the folder in the box below.
        SelectObject(dc, base);
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        const char* clientName = MANGOS_CLIENT_NAME;
        TextOutA(dc, tx, by + 34, clientName, int(std::strlen(clientName)));

        SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
        const char* blurb = "Tiles, collision, DBC and navmesh, straight from the client.";
        TextOutA(dc, tx, by + 52, blurb, int(std::strlen(blurb)));

        SelectObject(dc, oldFont);
        DeleteObject(title);
        (void)w;
    }

    void BuildUi(HWND w)
    {
        const int labelW = 130;
        const int editX = labelW + 20;
        const int editW = 400;
        const int btnX = editX + editW + 10;
        const int btnW = 90;
        int y = kBandH + 12;

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

        Add(w, "STATIC", "Language", SS_LEFT, editX + 100, y + 4, 70, 20, 0);
        // Tall on purpose: a combo's height is its DROPPED height, not the box you see.
        Add(w, "COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
            editX + 172, y, 110, 240, ID_LOCALE);
        Add(w, "BUTTON", "All languages", WS_TABSTOP | BS_AUTOCHECKBOX,
            editX + 292, y + 2, 120, 22, ID_CHK_ALLLOC);
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
        Add(w, "BUTTON", "Shut down the PC when finished", WS_TABSTOP | BS_AUTOCHECKBOX,
            256, cy + 32, 230, 22, ID_CHK_SHUTDOWN);
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

        // The gap left of the buttons, which was empty: when a nav bake runs for six
        // hours unattended, what it says is the only record of how long it took.
        Add(w, "STATIC", "", SS_LEFT, 14, y + 8, btnX + btnW - 220, 20, ID_TIMES);

        Add(w, "BUTTON", "Start", WS_TABSTOP | BS_DEFPUSHBUTTON,
            btnX + btnW - 200, y, 90, 28, ID_START);
        Add(w, "BUTTON", "Close", WS_TABSTOP | BS_PUSHBUTTON,
            btnX + btnW - 100, y, 90, 28, ID_CLOSE);

        Check(ID_CHK_DBC, true);
        Check(ID_CHK_GOMODELS, true);
        SetWindowTextA(Find(ID_SRC), (ExeDir() + "\\Data").c_str());
        SetWindowTextA(Find(ID_DEST), (ExeDir() + "\\extracted_data").c_str());

        // Both ship beside the exe and both are what the extractor would default to
        // anyway. Showing them beats an empty box that looks like something is missing.
        SetWindowTextA(Find(ID_VESSELS), (ExeDir() + "\\vessels.txt").c_str());
        SetWindowTextA(Find(ID_OFFMESH), (ExeDir() + "\\offmesh.txt").c_str());

        RefreshLocales();
    }

    LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
        {
            HDC dc = (HDC)wp;
            RECT client{};
            GetClientRect(w, &client);

            RECT below{0, kBandH, client.right, client.bottom};
            HBRUSH face = (HBRUSH)(COLOR_BTNFACE + 1);
            FillRect(dc, &below, face);

            PaintHeader(w, dc, client);
            return 1;
        }

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

            SYSTEMTIME done{};
            GetLocalTime(&done);
            const std::string span = "Started " + Clock(g_startedAt) +
                                     "   Finished " + Clock(done) +
                                     "   Took " + Elapsed(GetTickCount64() - g_startedTick);
            SetWindowTextA(Find(ID_TIMES), span.c_str());
            AppendLog(span);

            // ASKED FOR, AND STILL ASKED AGAIN. A six-hour bake ends while nobody is
            // watching, so the countdown is what a person who walked back in gets to
            // cancel -- and a failed run never triggers it at all.
            if (code == 0 && Checked(ID_CHK_SHUTDOWN))
            {
                AppendLog("-- shutting down in 60 seconds; run `shutdown /a` to stop it --");
                ShutdownPc();
            }
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
                    RefreshLocales();
                }
                return 0;
            case ID_SRC:
                if (HIWORD(wp) == EN_KILLFOCUS)
                {
                    RefreshLocales();
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
    // Resource 1, the icon Explorer already draws for this exe. Leaving it null
    // is what put the generic white page in the title bar and the task switcher.
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MangosExtractorGui";
    if (!RegisterClassA(&wc))
    {
        return 1;
    }

    RECT wanted{0, 0, 680, 680 + kBandH};
    AdjustWindowRect(&wanted, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                     FALSE);

    g_main = CreateWindowExA(0, wc.lpszClassName,
                             "MaNGOS client baker -- " MANGOS_CLIENT_NAME,
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
