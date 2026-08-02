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

// A dialog in front of mangosd.conf, for people who would rather not scroll two
// thousand lines of ini to change a port.
//
// IT KNOWS NOTHING ABOUT THE SERVER. It parses the conf, shows what is in it, and
// writes it back; the labels and the tooltips are generated from the file's own
// comment blocks. Nothing here is a second copy of what a setting means, so the
// editor cannot drift out of step with the server the way a hand-written form
// would. One conf gains a key, the editor shows it the next time it is opened.
//
// UTF-16 throughout: the -W half of the API, wWinMain, and no -A call anywhere.
// The narrow half decodes every string in the process code page, which mangles
// both the punctuation this window draws and any install path with a letter
// outside it -- a folder picker that cannot open C:\Users\<name> is not a folder
// picker. The conf file itself stays bytes; it is converted at the edge.

#include "ConfModel.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <uxtheme.h>

#include <cstring>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(linker, "/manifestdependency:\"type='win32' "                       \
                        "name='Microsoft.Windows.Common-Controls' "                  \
                        "version='6.0.0.0' processorArchitecture='*' "               \
                        "publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uxtheme.lib")
#endif

#ifndef MANGOS_CLIENT_NAME
#define MANGOS_CLIENT_NAME "unknown client"
#endif

namespace
{
    enum : int
    {
        ID_TABS = 1000,
        ID_OPEN, ID_SAVE, ID_SAVEAS, ID_RELOAD, ID_SEARCH, ID_SEARCH_GO, ID_THEME,
        ID_PAGE,
        ID_FIRST_FIELD = 2000     ///< one id per visible row, +1 for its Browse
    };

    const int HEADER_H = 62;
    const int TOOLBAR_H = 40;
    const int STATUS_H = 24;
    const int ROW_H = 30;
    const int LABEL_W = 300;
    const int FIELD_W = 260;
    const int MARGIN = 14;

    /**
     * @brief One palette; everything this window paints reads its colours here.
     *
     * @c classic strips the visual style off the common controls. A themed tab
     * or combo paints its own background from the system theme and ignores
     * WM_CTLCOLOR entirely, so on a dark palette it survives as a light patch --
     * the control has to be taken off the theme before it will take a colour.
     */
    struct Theme
    {
        const wchar_t* name;
        COLORREF pageBg, pageText, headingText;
        COLORREF fieldBg, fieldText;
        COLORREF chromeBg, chromeText, chromeEdge;
        COLORREF headerTop, headerBottom, headerText, headerSub, accent;
        COLORREF buttonBg, buttonHot, buttonText, buttonEdge;
        bool     classic;
    };

    const Theme THEMES[] =
    {
        {
            L"Light",
            RGB(255, 255, 255), RGB( 28,  28,  32), RGB( 28,  42,  74),
            RGB(255, 255, 255), RGB( 16,  16,  20),
            RGB(240, 240, 242), RGB( 60,  60,  66), RGB(206, 206, 210),
            RGB( 24,  34,  54), RGB( 40,  56,  84), RGB(240, 240, 245),
            RGB(168, 178, 196), RGB(196, 160,  82),
            RGB(252, 252, 253), RGB(232, 238, 248), RGB( 30,  30,  34),
            RGB(172, 172, 178),
            false
        },
        {
            L"Dark",
            RGB( 30,  31,  34), RGB(222, 224, 230), RGB(214, 178, 102),
            RGB( 45,  46,  50), RGB(236, 237, 240),
            RGB( 38,  39,  43), RGB(198, 200, 208), RGB( 58,  59,  64),
            RGB( 16,  20,  30), RGB( 30,  38,  58), RGB(238, 240, 246),
            RGB(150, 160, 182), RGB(196, 160,  82),
            RGB( 56,  57,  62), RGB( 74,  76,  84), RGB(228, 230, 236),
            RGB( 84,  86,  94),
            true
        },
        {
            // Workbench 1.3: four colours, and the orange was the one that moved.
            L"Amiga",
            RGB(  0,  85, 170), RGB(255, 255, 255), RGB(255, 136,   0),
            RGB(255, 255, 255), RGB(  0,   0,   0),
            RGB(255, 255, 255), RGB(  0,   0,   0), RGB(  0,   0,   0),
            RGB(  0,  85, 170), RGB(  0,  85, 170), RGB(255, 255, 255),
            RGB(255, 136,   0), RGB(255, 136,   0),
            RGB(255, 255, 255), RGB(255, 136,   0), RGB(  0,   0,   0),
            RGB(  0,   0,   0),
            true
        }
    };

    const int THEME_COUNT = (int)(sizeof(THEMES) / sizeof(THEMES[0]));

    /// KIND_CONNECTION is one semicolon-joined string; these are its fields.
    const wchar_t* const CONN_NAMES[] = { L"Host", L"Port", L"User", L"Password", L"Database" };
    const int CONN_WEIGHT[] = { 30, 12, 20, 20, 30 };
    const int CONN_PARTS = 5;

    struct Row
    {
        size_t entry;
        size_t section;
        HWND   label;
        HWND   field;      ///< null for KIND_CONNECTION, which uses parts instead
        HWND   browse;
        std::vector<HWND> parts;
        std::vector<HWND> captions;
    };

    conf::ConfFile    g_conf;
    std::vector<Row>  g_rows;
    std::vector<HWND> g_headings;
    HWND g_main = nullptr;
    HWND g_tabs = nullptr;
    HWND g_page = nullptr;
    HWND g_tip = nullptr;
    HWND g_search = nullptr;
    HWND g_themeButton = nullptr;
    HFONT g_font = nullptr;
    HFONT g_bold = nullptr;
    HFONT g_title = nullptr;
    HFONT g_small = nullptr;
    HBRUSH g_pageBrush = nullptr;
    HBRUSH g_fieldBrush = nullptr;
    HBRUSH g_chromeBrush = nullptr;
    std::wstring g_savePath;
    std::wstring g_statusText = L"Open a folder to begin";
    std::vector<std::wstring> g_tips;
    int  g_theme = 0;
    int  g_scroll = 0;
    int  g_pageHeight = 0;
    int  g_dpi = 96;
    bool g_loaded = false;

    const Theme& T() { return THEMES[g_theme]; }

    int Scale(int v) { return MulDiv(v, g_dpi, 96); }

    void DoScroll(int newPos);
    void LayoutChildren();
    void ApplyTheme();

    void Place(HWND h, int x, int y, int w, int cy)
    {
        if (h)
        {
            // SWP_NOCOPYBITS: never bit-blit old pixels into a STATIC. Scroll
            // trails of label text were the "garbled World tab" bug.
            SetWindowPos(h, nullptr, x, y, w, cy,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
        }
    }

    /// Content Y → client Y. Scroll by moving children, not ScrollWindowEx.
    int ViewY(int contentY)
    {
        return contentY - g_scroll;
    }

    /// Take a control off the visual style so WM_CTLCOLOR is what decides its colour.
    void StyleControl(HWND h)
    {
        if (!h)
        {
            return;
        }

        // Themed STATIC always paints its own light plate and ignores CTLCOLOR,
        // which is why labels looked grey on Light and unreadable on Dark/Amiga.
        // Always strip the style from statics; other controls only in classic palettes.
        wchar_t cls[32] = { 0 };
        GetClassNameW(h, cls, 32);
        const bool isStatic = _wcsicmp(cls, L"Static") == 0;
        const bool strip = isStatic || T().classic;
        SetWindowTheme(h, strip ? L"" : nullptr, strip ? L"" : nullptr);
    }

    /**
     * @brief Bytes from the conf to UTF-16 and back.
     *
     * The process code page, not UTF-8: these files are ASCII with the odd
     * Windows-1252 byte, and decoding them as UTF-8 would reject exactly those.
     * A value the user never touches is written back from the original bytes, so
     * only edited values pass through the conversion at all.
     */
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

    HFONT MakeFont(int weight, int extraPt)
    {
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        {
            return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }

        LOGFONTW lf = ncm.lfMessageFont;
        if (weight)
        {
            lf.lfWeight = weight;
        }
        if (extraPt)
        {
            lf.lfHeight -= MulDiv(extraPt, g_dpi, 72);
        }
        HFONT f = CreateFontIndirectW(&lf);
        return f ? f : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    /// @param visible false while a page is being built; see BuildPage.
    HWND Add(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
             int x, int y, int w, int h, int id, HFONT font = nullptr,
             bool visible = true)
    {
        HWND c = CreateWindowExW(0, cls, text,
                                 WS_CHILD | style | (visible ? WS_VISIBLE : 0),
                                 x, y, w, h, parent, (HMENU)(INT_PTR)id,
                                 GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
        StyleControl(c);
        return c;
    }

    /// Buttons are drawn by hand in every theme, so all three look like one program.
    HWND AddButton(HWND parent, const wchar_t* text, int x, int y, int w, int h,
                   int id, bool visible = true)
    {
        return Add(parent, L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP,
                   x, y, w, h, id, nullptr, visible);
    }

    void DrawButton(const DRAWITEMSTRUCT* di)
    {
        const Theme& t = T();
        const bool down = (di->itemState & ODS_SELECTED) != 0;

        RECT rc = di->rcItem;
        HBRUSH face = CreateSolidBrush(down ? t.buttonHot : t.buttonBg);
        FillRect(di->hDC, &rc, face);
        DeleteObject(face);

        HBRUSH edge = CreateSolidBrush((di->itemState & ODS_FOCUS) ? t.accent : t.buttonEdge);
        FrameRect(di->hDC, &rc, edge);
        DeleteObject(edge);

        wchar_t text[128] = { 0 };
        GetWindowTextW(di->hwndItem, text, 128);

        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, (di->itemState & ODS_DISABLED) ? t.chromeEdge : t.buttonText);
        HGDIOBJ old = SelectObject(di->hDC, g_font);
        DrawTextW(di->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(di->hDC, old);
    }

    void DrawTab(const DRAWITEMSTRUCT* di)
    {
        const Theme& t = T();
        const bool current = (di->itemState & ODS_SELECTED) != 0;

        RECT rc = di->rcItem;
        HBRUSH face = CreateSolidBrush(current ? t.pageBg : t.chromeBg);
        FillRect(di->hDC, &rc, face);
        DeleteObject(face);

        if (current)
        {
            RECT bar{ rc.left, rc.top, rc.right, rc.top + Scale(3) };
            HBRUSH accent = CreateSolidBrush(t.accent);
            FillRect(di->hDC, &bar, accent);
            DeleteObject(accent);
        }

        wchar_t text[64] = { 0 };
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = text;
        item.cchTextMax = 64;
        SendMessageW(di->hwndItem, TCM_GETITEMW, di->itemID, (LPARAM)&item);

        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, current ? t.pageText : t.chromeText);
        HGDIOBJ old = SelectObject(di->hDC, current ? g_bold : g_font);
        DrawTextW(di->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(di->hDC, old);
    }

    std::wstring TextOf(HWND h)
    {
        if (!h)
        {
            return std::wstring();
        }
        const int n = GetWindowTextLengthW(h);
        std::wstring s((size_t)n + 1, L'\0');
        GetWindowTextW(h, &s[0], n + 1);
        s.resize((size_t)n);
        return s;
    }

    /// The status strip is painted with the window, not a common control: a
    /// status bar takes its background from the system theme and stays light.
    void SetStatus(const std::wstring& text)
    {
        g_statusText = text;
        if (!g_main)
        {
            return;
        }

        RECT rc{};
        GetClientRect(g_main, &rc);
        RECT strip{ 0, rc.bottom - Scale(STATUS_H), rc.right, rc.bottom };
        InvalidateRect(g_main, &strip, FALSE);
    }

    std::wstring EnumTextFor(const conf::Entry& e, const std::string& value)
    {
        for (size_t i = 0; i < e.optionValues.size(); ++i)
        {
            if (e.optionValues[i] == value)
            {
                return Widen(value) + L"  \x2014  " + Widen(e.optionLabels[i]);
            }
        }
        return Widen(value);
    }

    std::string EnumValueFrom(const conf::Entry& e, const std::wstring& text)
    {
        for (size_t i = 0; i < e.optionValues.size(); ++i)
        {
            if (EnumTextFor(e, e.optionValues[i]) == text)
            {
                return e.optionValues[i];
            }
        }

        const std::string typed = Narrow(text);
        const size_t cut = typed.find(' ');
        return (cut == std::string::npos) ? typed : typed.substr(0, cut);
    }

    /// The model joins the dotted parts of a key with ASCII; the dot is drawn here.
    std::wstring Label(const std::string& key)
    {
        std::wstring text = Widen(conf::Humanize(key));
        for (size_t at = text.find(L" - "); at != std::wstring::npos;
             at = text.find(L" - ", at + 1))
        {
            text.replace(at, 3, L" \x00B7 ");
        }
        return text;
    }

    std::wstring FileName(const std::wstring& path)
    {
        const size_t cut = path.find_last_of(L"\\/");
        return (cut == std::wstring::npos) ? path : path.substr(cut + 1);
    }

    void UpdateTitle()
    {
        std::wstring title = L"MaNGOS Config Editor";
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

    bool PickFolder(HWND owner, const wchar_t* title, std::wstring& out)
    {
        BROWSEINFOW info{};
        wchar_t display[MAX_PATH] = { 0 };
        info.hwndOwner = owner;
        info.pszDisplayName = display;
        info.lpszTitle = title;
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

    bool SaveAsDialog(HWND owner, std::wstring& out)
    {
        wchar_t chosen[MAX_PATH] = { 0 };
        lstrcpynW(chosen, out.c_str(), MAX_PATH);

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"Config files\0*.conf;*.dist;*.in\0All files\0*.*\0\0";
        ofn.lpstrFile = chosen;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Save configuration as";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&ofn))
        {
            return false;
        }
        out = chosen;
        return true;
    }

    bool Exists(const std::wstring& path)
    {
        const DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
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

    bool EndsWith(const std::wstring& s, const wchar_t* tail)
    {
        const size_t n = wcslen(tail);
        return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
    }

    int ConfRank(const std::wstring& name)
    {
        // Prefer live conf, then packaged dist, then CMake input; mangosd before realmd.
        int tier = 9;
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
        else
        {
            return 100;
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
        return tier * 10 + who;
    }

    /// Prefer mangosd.conf, then .dist / .dist.in; also picks any other *.conf* present.
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
            const std::wstring glob = folder + L"\\" + pat;
            const HANDLE h = FindFirstFileW(glob.c_str(), &fd);
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
                const int rank = ConfRank(fd.cFileName);
                if (rank < bestRank)
                {
                    bestRank = rank;
                    best = folder + L"\\" + fd.cFileName;
                }
            }
            while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        return best;
    }

    /// Walk up from the exe: tools/confeditor → tools → install root.
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

    /**
     * @brief Where Save writes, which is not always where Load read.
     *
     * Opening a folder that only ships the template must not turn the editor into
     * a template editor: a .dist is the packaged default, and the server reads
     * the .conf beside it. So the template is the starting point and the .conf is
     * the destination, which is exactly what an admin does by hand anyway.
     */
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

    void AddTip(HWND control, const std::wstring& text)
    {
        if (!g_tip || !control || text.empty())
        {
            return;
        }

        // The tooltip keeps the pointer, not the text, so the strings have to
        // outlive the call; they die with the page.
        g_tips.push_back(text);

        TOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = g_page;
        ti.uId = (UINT_PTR)control;
        ti.lpszText = (LPWSTR)g_tips.back().c_str();
        SendMessageW(g_tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }

    void Harvest()
    {
        for (const Row& r : g_rows)
        {
            const conf::Entry& e = g_conf.Entries()[r.entry];
            std::string value;

            switch (e.kind)
            {
                case conf::KIND_BOOL:
                {
                    const bool on = SendMessageW(r.field, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    const bool words = e.value == "true" || e.value == "false";
                    value = words ? (on ? "true" : "false") : (on ? "1" : "0");
                    break;
                }
                case conf::KIND_CHOICE:
                {
                    const int sel = (int)SendMessageW(r.field, CB_GETCURSEL, 0, 0);
                    value = (sel >= 0 && sel < (int)e.choices.size()) ? e.choices[sel] : e.value;
                    break;
                }
                case conf::KIND_ENUM:
                {
                    const int sel = (int)SendMessageW(r.field, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)e.optionValues.size())
                    {
                        value = e.optionValues[sel];
                    }
                    else
                    {
                        value = EnumValueFrom(e, TextOf(r.field));
                    }
                    break;
                }
                case conf::KIND_CONNECTION:
                {
                    for (size_t p = 0; p < r.parts.size(); ++p)
                    {
                        value += Narrow(TextOf(r.parts[p]));
                        if (p + 1 < r.parts.size())
                        {
                            value += ';';
                        }
                    }
                    break;
                }
                default:
                    value = Narrow(TextOf(r.field));
                    break;
            }

            if (g_conf.SetValue(r.entry, value))
            {
                UpdateTitle();
            }
        }
    }

    void DestroyPage()
    {
        if (g_tip)
        {
            DestroyWindow(g_tip);
            g_tip = nullptr;
        }
        for (const Row& r : g_rows)
        {
            DestroyWindow(r.label);
            if (r.field)
            {
                DestroyWindow(r.field);
            }
            if (r.browse)
            {
                DestroyWindow(r.browse);
            }
            for (HWND h : r.parts)
            {
                DestroyWindow(h);
            }
            for (HWND h : r.captions)
            {
                DestroyWindow(h);
            }
        }
        g_rows.clear();

        for (HWND h : g_headings)
        {
            DestroyWindow(h);
        }
        g_headings.clear();
        g_tips.clear();
    }

    HWND FocusTarget(const Row& r)
    {
        if (r.field)
        {
            return r.field;
        }
        return r.parts.empty() ? r.label : r.parts[0];
    }

    /**
     * @brief Place every control for the current page width and scroll offset.
     *
     * Scroll is applied here as ViewY (contentY - g_scroll), not via
     * ScrollWindowEx. Bit-blitting STATIC children leaves trails of every
     * label they ever showed -- the World tab after a few wheel ticks.
     */
    void LayoutRows()
    {
        RECT rc{};
        GetClientRect(g_page, &rc);

        const int usable = rc.right - Scale(2 * MARGIN);
        if (usable <= 0 || g_rows.empty())
        {
            return;
        }

        const int gap = Scale(28);
        int columns = (usable + gap) / (Scale(LABEL_W + FIELD_W) + gap);
        columns = (columns < 1) ? 1 : (columns > 3 ? 3 : columns);

        const int columnW = (usable - (columns - 1) * gap) / columns;

        // A column wider than a label plus a field is padding, not room: letting
        // the field grow into it drags the control away from the label it belongs
        // to, and a checkbox ends up marooned half a screen from its name.
        int contentW = Scale(LABEL_W + FIELD_W);
        if (contentW > columnW)
        {
            contentW = columnW;
        }

        int labelW = (contentW * 55) / 100;
        if (labelW > Scale(LABEL_W))
        {
            labelW = Scale(LABEL_W);
        }
        const int fieldW = contentW - labelW - Scale(10);

        // Clamp against the last measured height before placing, so ViewY is stable.
        {
            const int maxPos = (g_pageHeight > rc.bottom) ? g_pageHeight - rc.bottom : 0;
            if (g_scroll > maxPos)
            {
                g_scroll = maxPos;
            }
        }

        int y = Scale(MARGIN);
        int column = 0;
        int rowTop = y;
        size_t shown = (size_t)-1;
        size_t heading = 0;

        for (const Row& r : g_rows)
        {
            if (r.section != shown)
            {
                shown = r.section;
                if (column)
                {
                    rowTop += Scale(ROW_H);
                }
                y = rowTop + (heading ? Scale(12) : 0);

                if (heading < g_headings.size())
                {
                    Place(g_headings[heading], Scale(MARGIN), ViewY(y), usable, Scale(20));
                }
                ++heading;

                y += Scale(26);
                rowTop = y;
                column = 0;
            }

            const conf::Kind kind = g_conf.Entries()[r.entry].kind;

            // Five fields want the full row; a column layout would crush them.
            if (kind == conf::KIND_CONNECTION)
            {
                if (column)
                {
                    rowTop += Scale(ROW_H);
                    column = 0;
                }

                Place(r.label, Scale(MARGIN), ViewY(rowTop + Scale(14)), labelW, Scale(20));

                const int fx = Scale(MARGIN) + labelW + Scale(10);
                const int total = usable - labelW - Scale(10);
                const int partGap = Scale(6);
                int weightSum = 0;
                for (int p = 0; p < CONN_PARTS; ++p)
                {
                    weightSum += CONN_WEIGHT[p];
                }

                int x = fx;
                for (int p = 0; p < CONN_PARTS; ++p)
                {
                    int w = (total - (CONN_PARTS - 1) * partGap) * CONN_WEIGHT[p] / weightSum;
                    if (w < Scale(40))
                    {
                        w = Scale(40);
                    }
                    Place(r.captions[p], x, ViewY(rowTop), w, Scale(12));
                    Place(r.parts[p], x, ViewY(rowTop + Scale(14)), w, Scale(23));
                    x += w + partGap;
                }

                rowTop += Scale(ROW_H) + Scale(14);
                continue;
            }

            const int x = Scale(MARGIN) + column * (columnW + gap);
            Place(r.label, x, ViewY(rowTop + Scale(4)), labelW, Scale(20));

            const int fx = x + labelW + Scale(10);
            if (r.browse)
            {
                Place(r.field, fx, ViewY(rowTop), fieldW - Scale(34), Scale(23));
                Place(r.browse, fx + fieldW - Scale(30), ViewY(rowTop), Scale(30), Scale(23));
            }
            else
            {
                // A combo box is sized including its dropped list, not just the
                // closed edit: give it a short one and the list has no room.
                const int h = (kind == conf::KIND_CHOICE || kind == conf::KIND_ENUM)
                              ? Scale(240) : Scale(23);
                const int w = (kind == conf::KIND_BOOL) ? Scale(24) : fieldW;
                Place(r.field, fx,
                      ViewY(rowTop + (kind == conf::KIND_BOOL ? Scale(2) : 0)),
                      w, h);
            }

            if (++column >= columns)
            {
                column = 0;
                rowTop += Scale(ROW_H);
            }
        }

        if (column)
        {
            rowTop += Scale(ROW_H);
        }
        g_pageHeight = rowTop + Scale(MARGIN);

        // Keep the thumb inside the new range (e.g. after a narrower window).
        const int maxPos = (g_pageHeight > rc.bottom) ? g_pageHeight - rc.bottom : 0;
        if (g_scroll > maxPos)
        {
            g_scroll = maxPos;
        }

        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = g_pageHeight;
        si.nPage = (UINT)(rc.bottom - rc.top);
        si.nPos = g_scroll;
        SetScrollInfo(g_page, SB_VERT, &si, TRUE);

        RedrawWindow(g_page, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }

    std::vector<std::string> SplitConnection(const std::string& value)
    {
        std::vector<std::string> parts;
        size_t at = 0;
        for (int p = 0; p < CONN_PARTS; ++p)
        {
            const size_t cut = value.find(';', at);
            parts.push_back(value.substr(at, cut == std::string::npos ? std::string::npos : cut - at));
            if (cut == std::string::npos)
            {
                break;
            }
            at = cut + 1;
        }
        parts.resize(CONN_PARTS);
        return parts;
    }

    void BuildPage(size_t group)
    {
        DestroyPage();
        g_scroll = 0;

        g_tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                0, 0, 0, 0, g_page, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_tip, TTM_SETMAXTIPWIDTH, 0, Scale(460));
        SendMessageW(g_tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELPARAM(30000, 0));
        SendMessageW(g_tip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELPARAM(350, 0));

        const std::vector<conf::Entry>& entries = g_conf.Entries();
        const std::vector<conf::Section>& sections = g_conf.Sections();
        int id = ID_FIRST_FIELD;
        size_t shownSection = (size_t)-1;

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const conf::Entry& e = entries[i];
            if (e.group != group)
            {
                continue;
            }

            // Several banners share a tab, so each keeps its own heading and the
            // file's own grouping survives being folded into fewer tabs.
            if (e.section != shownSection)
            {
                shownSection = e.section;
                const std::wstring head = Widen(sections[e.section].title);
                g_headings.push_back(Add(g_page, L"STATIC", head.c_str(), SS_LEFT,
                                         0, 0, 10, 10, 0, g_bold, false));
            }

            Row row{};
            row.entry = i;
            row.section = e.section;
            row.browse = nullptr;
            row.field = nullptr;

            const std::wstring label = Label(e.key);
            row.label = Add(g_page, L"STATIC", label.c_str(),
                            SS_LEFT | SS_NOTIFY | SS_ENDELLIPSIS, 0, 0, 10, 10, 0, nullptr, false);

            const std::wstring value = Widen(e.value);

            switch (e.kind)
            {
                case conf::KIND_BOOL:
                {
                    row.field = Add(g_page, L"BUTTON", L"", BS_AUTOCHECKBOX | WS_TABSTOP,
                                    0, 0, 10, 10, id, nullptr, false);
                    const bool on = e.value == "1" || e.value == "true";
                    SendMessageW(row.field, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
                    break;
                }
                case conf::KIND_ENUM:
                {
                    // Closed list: CBS_DROPDOWN keeps an edit that Windows fully
                    // selects on focus, which looks like a random pre-selection.
                    row.field = Add(g_page, L"COMBOBOX", L"",
                                    CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                                    0, 0, 10, 10, id, nullptr, false);
                    int sel = -1;
                    for (size_t o = 0; o < e.optionValues.size(); ++o)
                    {
                        const std::wstring item = EnumTextFor(e, e.optionValues[o]);
                        SendMessageW(row.field, CB_ADDSTRING, 0, (LPARAM)item.c_str());
                        if (e.optionValues[o] == e.value)
                        {
                            sel = (int)o;
                        }
                    }
                    if (sel < 0)
                    {
                        const std::wstring cur = EnumTextFor(e, e.value);
                        SendMessageW(row.field, CB_ADDSTRING, 0, (LPARAM)cur.c_str());
                        sel = (int)e.optionValues.size();
                    }
                    SendMessageW(row.field, CB_SETCURSEL, (WPARAM)sel, 0);
                    break;
                }
                case conf::KIND_CHOICE:
                {
                    row.field = Add(g_page, L"COMBOBOX", L"",
                                    CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                                    0, 0, 10, 10, id, nullptr, false);
                    for (const std::string& c : e.choices)
                    {
                        const std::wstring w = Widen(c);
                        SendMessageW(row.field, CB_ADDSTRING, 0, (LPARAM)w.c_str());
                    }
                    const int sel = (int)SendMessageW(row.field, CB_FINDSTRINGEXACT,
                                                      (WPARAM)-1, (LPARAM)value.c_str());
                    SendMessageW(row.field, CB_SETCURSEL, (WPARAM)(sel >= 0 ? sel : 0), 0);
                    break;
                }
                case conf::KIND_PATH:
                {
                    row.field = Add(g_page, L"EDIT", value.c_str(),
                                    WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 10, 10, id, nullptr, false);
                    row.browse = AddButton(g_page, L"\x2026", 0, 0, 10, 10, id + 1, false);
                    break;
                }
                case conf::KIND_CONNECTION:
                {
                    const std::vector<std::string> parts = SplitConnection(e.value);
                    for (int p = 0; p < CONN_PARTS; ++p)
                    {
                        row.captions.push_back(Add(g_page, L"STATIC", CONN_NAMES[p], SS_LEFT,
                                                   0, 0, 10, 10, 0, g_small, false));
                        row.parts.push_back(Add(g_page, L"EDIT", Widen(parts[p]).c_str(),
                                                WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                                0, 0, 10, 10, 0, nullptr, false));
                    }
                    break;
                }
                default:
                {
                    const DWORD extra = (e.kind == conf::KIND_NUMBER) ? ES_RIGHT : 0;
                    row.field = Add(g_page, L"EDIT", value.c_str(),
                                    WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | extra,
                                    0, 0, 10, 10, id, nullptr, false);
                    break;
                }
            }

            std::wstring tip = Widen(e.key);
            if (!e.doc.empty())
            {
                tip += L"\r\n\r\n" + Widen(e.doc);
            }
            AddTip(row.field, tip);
            AddTip(row.label, tip);
            for (HWND h : row.parts)
            {
                AddTip(h, tip);
            }

            g_rows.push_back(row);
            id += 2;
        }

        SetScrollPos(g_page, SB_VERT, 0, TRUE);
        LayoutRows();

        // Only now are they worth looking at. Created visible, every control
        // paints once at 0,0 before the layout moves it, so switching tabs
        // flickers a heap of overlapping labels in the top-left corner.
        for (const Row& r : g_rows)
        {
            ShowWindow(r.label, SW_SHOWNA);
            if (r.field)
            {
                ShowWindow(r.field, SW_SHOWNA);
            }
            if (r.browse)
            {
                ShowWindow(r.browse, SW_SHOWNA);
            }
            for (HWND h : r.parts)
            {
                ShowWindow(h, SW_SHOWNA);
            }
            for (HWND h : r.captions)
            {
                ShowWindow(h, SW_SHOWNA);
            }
        }
        for (HWND h : g_headings)
        {
            ShowWindow(h, SW_SHOWNA);
        }

        wchar_t buf[160];
        wsprintfW(buf, L"%d settings in %d section(s) on this tab \x2014 %d in the file",
                  (int)g_rows.size(), (int)g_headings.size(),
                  (int)g_conf.Entries().size());
        SetStatus(buf);
    }

    void SelectGroup(size_t group)
    {
        const int count = (int)SendMessageW(g_tabs, TCM_GETITEMCOUNT, 0, 0);
        for (int i = 0; i < count; ++i)
        {
            TCITEMW item{};
            item.mask = TCIF_PARAM;
            SendMessageW(g_tabs, TCM_GETITEMW, i, (LPARAM)&item);
            if ((size_t)item.lParam == group)
            {
                SendMessageW(g_tabs, TCM_SETCURSEL, i, 0);
                BuildPage(group);
                return;
            }
        }
    }

    void RebuildTabs()
    {
        SendMessageW(g_tabs, TCM_DELETEALLITEMS, 0, 0);

        const std::vector<conf::Section>& sections = g_conf.Sections();
        const std::vector<std::string>& groups = g_conf.Groups();

        std::vector<int> count(groups.size(), 0);
        for (const conf::Entry& e : g_conf.Entries())
        {
            count[e.group]++;
        }

        int index = 0;
        size_t first = 0;
        bool haveFirst = false;
        for (size_t g = 0; g < groups.size(); ++g)
        {
            if (!count[g])
            {
                continue;
            }

            wchar_t text[64];
            wsprintfW(text, L"%s (%d)", Widen(groups[g]).c_str(), count[g]);

            TCITEMW item{};
            item.mask = TCIF_TEXT | TCIF_PARAM;
            item.pszText = text;
            item.lParam = (LPARAM)g;
            SendMessageW(g_tabs, TCM_INSERTITEMW, index++, (LPARAM)&item);

            if (!haveFirst)
            {
                first = g;
                haveFirst = true;
            }
        }

        // Only now does TCM_ADJUSTRECT know a tab row exists. Asked before the
        // first item is inserted it reserves the border alone, and the page is
        // placed over the tab labels -- which looks exactly like having no tabs.
        LayoutChildren();

        if (haveFirst)
        {
            SelectGroup(first);
        }
    }

    void LayoutChildren()
    {
        RECT rc{};
        GetClientRect(g_main, &rc);

        const int top = Scale(HEADER_H) + Scale(TOOLBAR_H);
        const int bottom = rc.bottom - Scale(STATUS_H);

        if (g_themeButton)
        {
            Place(g_themeButton, rc.right - Scale(MARGIN) - Scale(90),
                  Scale(HEADER_H) + Scale(7), Scale(90), Scale(26));
        }

        MoveWindow(g_tabs, Scale(8), top, rc.right - Scale(16), bottom - top - Scale(8), TRUE);

        RECT tabRect{ 0, 0, rc.right - Scale(16), bottom - top - Scale(8) };
        SendMessageW(g_tabs, TCM_ADJUSTRECT, FALSE, (LPARAM)&tabRect);
        MoveWindow(g_page, Scale(8) + tabRect.left, top + tabRect.top,
                   tabRect.right - tabRect.left, tabRect.bottom - tabRect.top, TRUE);

        if (!g_rows.empty())
        {
            LayoutRows();
        }
    }

    void DoScroll(int newPos)
    {
        RECT rc{};
        GetClientRect(g_page, &rc);
        const int maxPos = (g_pageHeight > rc.bottom) ? g_pageHeight - rc.bottom : 0;

        newPos = (newPos < 0) ? 0 : (newPos > maxPos ? maxPos : newPos);
        if (newPos == g_scroll)
        {
            return;
        }

        g_scroll = newPos;
        LayoutRows();
    }

    void ApplyTheme()
    {
        if (g_pageBrush)
        {
            DeleteObject(g_pageBrush);
        }
        if (g_fieldBrush)
        {
            DeleteObject(g_fieldBrush);
        }
        if (g_chromeBrush)
        {
            DeleteObject(g_chromeBrush);
        }
        g_pageBrush = CreateSolidBrush(T().pageBg);
        g_fieldBrush = CreateSolidBrush(T().fieldBg);
        g_chromeBrush = CreateSolidBrush(T().chromeBg);

        // Class brush is what ScrollWindowEx / BeginPaint use when they do not
        // go through our WM_ERASEBKGND path; keep it in step with the palette.
        if (g_page)
        {
            SetClassLongPtrW(g_page, GCLP_HBRBACKGROUND, (LONG_PTR)g_pageBrush);
        }

        if (g_themeButton)
        {
            std::wstring caption = L"Theme: ";
            caption += T().name;
            SetWindowTextW(g_themeButton, caption.c_str());
        }

        // Only common controls need SetWindowTheme. The custom page paints itself;
        // untheming it leaves the default light client colour showing through.
        auto restyle = [](HWND h)
        {
            if (h)
            {
                StyleControl(h);
            }
        };
        restyle(g_tabs);
        restyle(g_search);
        for (HWND child = GetWindow(g_main, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            if (child == g_page)
            {
                continue;
            }
            restyle(child);
        }
        for (const Row& r : g_rows)
        {
            // Statics must leave the visual style too, or they paint a light
            // plate and ignore WM_CTLCOLORSTATIC on a dark page.
            restyle(r.label);
            restyle(r.field);
            restyle(r.browse);
            for (HWND h : r.parts)
            {
                restyle(h);
            }
            for (HWND h : r.captions)
            {
                restyle(h);
            }
        }
        for (HWND h : g_headings)
        {
            restyle(h);
        }

        InvalidateRect(g_main, nullptr, TRUE);
        if (g_page)
        {
            RedrawWindow(g_page, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        if (g_tabs)
        {
            InvalidateRect(g_tabs, nullptr, TRUE);
        }
    }

    void CycleTheme()
    {
        g_theme = (g_theme + 1) % THEME_COUNT;
        ApplyTheme();
        SetStatus(std::wstring(L"Theme: ") + T().name);
    }

    void OpenPath(const std::wstring& path)
    {
        std::string error;
        if (!g_conf.Load(Narrow(path), error))
        {
            MessageBoxW(g_main, Widen(error).c_str(), L"MaNGOS Config Editor",
                        MB_OK | MB_ICONWARNING);
            return;
        }

        g_loaded = true;
        g_savePath = SaveTargetFor(path);
        RebuildTabs();
        ApplyTheme();
        UpdateTitle();
        InvalidateRect(g_main, nullptr, TRUE);

        // @NAME@ means CMake never substituted this file. Saving it as the live
        // conf hands the server a ConfVersion it cannot parse, and the failure
        // surfaces far away from here.
        int placeholders = 0;
        for (const conf::Entry& e : g_conf.Entries())
        {
            if (e.value.size() > 2 && e.value.front() == '@' && e.value.back() == '@')
            {
                ++placeholders;
            }
        }

        if (placeholders)
        {
            wchar_t warn[200];
            wsprintfW(warn, L"%d value(s) are still CMake placeholders (@NAME@) \x2014 "
                            L"this file was never configured by the build",
                      placeholders);
            SetStatus(warn);
        }
        else if (g_savePath != path)
        {
            SetStatus(L"Read the template " + FileName(path) +
                      L" \x2014 Save writes " + FileName(g_savePath));
        }
    }

    void OnOpenFolder()
    {
        std::wstring folder;
        if (!PickFolder(g_main, L"Pick the folder holding mangosd.conf", folder))
        {
            return;
        }

        const std::wstring found = FindConf(folder);
        if (found.empty())
        {
            MessageBoxW(g_main,
                        L"No mangosd.conf, mangosd.conf.dist or realmd.conf in that folder.",
                        L"MaNGOS Config Editor", MB_OK | MB_ICONINFORMATION);
            return;
        }
        OpenPath(found);
    }

    bool DoSave(const std::wstring& path)
    {
        Harvest();

        std::string error;
        if (!g_conf.Save(Narrow(path), error))
        {
            MessageBoxW(g_main, Widen(error).c_str(), L"MaNGOS Config Editor",
                        MB_OK | MB_ICONERROR);
            return false;
        }

        g_savePath = path;
        g_conf.ClearDirty();
        UpdateTitle();
        SetStatus(L"Saved " + path);
        return true;
    }

    std::wstring Lowered(std::wstring s)
    {
        if (!s.empty())
        {
            CharLowerBuffW(&s[0], (DWORD)s.size());
        }
        return s;
    }

    void OnSearch()
    {
        if (!g_loaded)
        {
            return;
        }

        const std::wstring typed = TextOf(g_search);
        if (typed.empty())
        {
            return;
        }
        const std::wstring needle = Lowered(typed);

        const std::vector<conf::Entry>& entries = g_conf.Entries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const std::wstring key = Lowered(Widen(entries[i].key));
            const std::wstring label = Lowered(Label(entries[i].key));
            const std::wstring doc = Lowered(Widen(entries[i].doc));

            if (key.find(needle) == std::wstring::npos &&
                label.find(needle) == std::wstring::npos &&
                doc.find(needle) == std::wstring::npos)
            {
                continue;
            }

            Harvest();
            SelectGroup(entries[i].group);

            for (const Row& r : g_rows)
            {
                if (r.entry != i)
                {
                    continue;
                }
                HWND target = FocusTarget(r);
                RECT rc{};
                GetWindowRect(target, &rc);
                POINT pt{ 0, rc.top };
                ScreenToClient(g_page, &pt);
                DoScroll(g_scroll + pt.y - Scale(MARGIN));
                SetFocus(target);
                SetStatus(L"Found " + Widen(entries[i].key));
                return;
            }
            return;
        }

        SetStatus(L"No setting matches \x201C" + typed + L"\x201D");
    }

    bool ConfirmDiscard()
    {
        Harvest();
        if (!g_conf.Dirty())
        {
            return true;
        }

        const int answer = MessageBoxW(g_main, L"Save the changes first?",
                                       L"MaNGOS Config Editor",
                                       MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL)
        {
            return false;
        }
        if (answer == IDYES)
        {
            return DoSave(g_savePath);
        }
        return true;
    }

    void PaintHeader(HDC dc, const RECT& client)
    {
        const Theme& t = T();
        RECT band{ 0, 0, client.right, Scale(HEADER_H) };

        // Hand-rolled vertical blend: one gradient is not worth a dependency on
        // msimg32 just for GradientFill.
        for (int y = band.top; y < band.bottom; ++y)
        {
            const int u = MulDiv(y - band.top, 255, (band.bottom - band.top));
            const int r = GetRValue(t.headerTop) +
                          MulDiv(u, GetRValue(t.headerBottom) - GetRValue(t.headerTop), 255);
            const int g = GetGValue(t.headerTop) +
                          MulDiv(u, GetGValue(t.headerBottom) - GetGValue(t.headerTop), 255);
            const int b = GetBValue(t.headerTop) +
                          MulDiv(u, GetBValue(t.headerBottom) - GetBValue(t.headerTop), 255);
            RECT line{ 0, y, client.right, y + 1 };
            HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
            FillRect(dc, &line, brush);
            DeleteObject(brush);
        }

        RECT accent{ 0, band.bottom - Scale(2), client.right, band.bottom };
        HBRUSH gold = CreateSolidBrush(t.accent);
        FillRect(dc, &accent, gold);
        DeleteObject(gold);

        SetBkMode(dc, TRANSPARENT);

        RECT text{ Scale(MARGIN), Scale(10), client.right - Scale(MARGIN), Scale(34) };
        SetTextColor(dc, t.headerText);
        HGDIOBJ old = SelectObject(dc, g_title);
        DrawTextW(dc, L"MaNGOS server configuration", -1, &text,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(dc, old);

        RECT sub{ Scale(MARGIN), Scale(34), client.right - Scale(MARGIN),
                  Scale(HEADER_H) - Scale(4) };
        SetTextColor(dc, t.headerSub);
        old = SelectObject(dc, g_font);
        const std::wstring where =
            g_loaded ? Widen(g_conf.Path())
                     : std::wstring(L"Open the folder that holds mangosd.conf \x2014 ") +
                       Widen(MANGOS_CLIENT_NAME);
        DrawTextW(dc, where.c_str(), -1, &sub,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS);
        SelectObject(dc, old);
    }

    void PaintStatus(HDC dc, const RECT& client)
    {
        const Theme& t = T();
        RECT strip{ 0, client.bottom - Scale(STATUS_H), client.right, client.bottom };
        FillRect(dc, &strip, g_chromeBrush ? g_chromeBrush : GetSysColorBrush(COLOR_BTNFACE));

        RECT edge{ strip.left, strip.top, strip.right, strip.top + 1 };
        HBRUSH line = CreateSolidBrush(t.chromeEdge);
        FillRect(dc, &edge, line);
        DeleteObject(line);

        RECT text{ Scale(MARGIN), strip.top, strip.right - Scale(MARGIN), strip.bottom };
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, t.chromeText);
        HGDIOBJ old = SelectObject(dc, g_font);
        DrawTextW(dc, g_statusText.c_str(), -1, &text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, old);
    }

    bool IsHeading(HWND h)
    {
        for (HWND head : g_headings)
        {
            if (head == h)
            {
                return true;
            }
        }
        return false;
    }

    LRESULT ColorFor(HDC dc, HWND control, bool edit)
    {
        const Theme& t = T();
        if (edit)
        {
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, t.fieldBg);
            SetTextColor(dc, t.fieldText);
            return (LRESULT)g_fieldBrush;
        }

        // Opaque page colour: same as the page fill, so the label slab vanishes
        // into the background with readable text on every palette.
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, t.pageBg);
        SetTextColor(dc, IsHeading(control) ? t.headingText : t.pageText);
        return (LRESULT)g_pageBrush;
    }

    LRESULT CALLBACK PageProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
            case WM_VSCROLL:
            {
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_ALL;
                GetScrollInfo(w, SB_VERT, &si);

                int pos = g_scroll;
                switch (LOWORD(wp))
                {
                    case SB_TOP:           pos = 0; break;
                    case SB_BOTTOM:        pos = si.nMax; break;
                    case SB_LINEUP:        pos -= Scale(ROW_H); break;
                    case SB_LINEDOWN:      pos += Scale(ROW_H); break;
                    case SB_PAGEUP:        pos -= (int)si.nPage; break;
                    case SB_PAGEDOWN:      pos += (int)si.nPage; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                    default: break;
                }
                DoScroll(pos);
                return 0;
            }

            case WM_MOUSEWHEEL:
            {
                const int delta = GET_WHEEL_DELTA_WPARAM(wp);
                DoScroll(g_scroll - (delta * Scale(ROW_H) * 3) / WHEEL_DELTA);
                return 0;
            }

            case WM_COMMAND:
            {
                const int id = LOWORD(wp);
                const int code = HIWORD(wp);
                if (code == BN_CLICKED && id >= ID_FIRST_FIELD && (id & 1))
                {
                    for (const Row& r : g_rows)
                    {
                        if (!r.browse || GetDlgCtrlID(r.browse) != id)
                        {
                            continue;
                        }
                        std::wstring folder = TextOf(r.field);
                        if (PickFolder(g_main, L"Pick a directory", folder))
                        {
                            SetWindowTextW(r.field, folder.c_str());
                            Harvest();
                        }
                        return 0;
                    }
                }
                break;
            }

            case WM_DRAWITEM:
            {
                const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)lp;
                if (di->CtlType == ODT_BUTTON)
                {
                    DrawButton(di);
                    return TRUE;
                }
                break;
            }

            case WM_CTLCOLORSTATIC:
                return ColorFor((HDC)wp, (HWND)lp, false);

            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
                return ColorFor((HDC)wp, (HWND)lp, true);

            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(w, &ps);
                if (g_pageBrush)
                {
                    FillRect(dc, &ps.rcPaint, g_pageBrush);
                }
                EndPaint(w, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
            {
                RECT rc{};
                GetClientRect(w, &rc);
                FillRect((HDC)wp, &rc, g_pageBrush ? g_pageBrush : GetSysColorBrush(COLOR_WINDOW));
                return 1;
            }

            default:
                break;
        }
        return DefWindowProcW(w, msg, wp, lp);
    }

    LRESULT CALLBACK MainProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
            case WM_CREATE:
            {
                g_main = w;

                const int bx = Scale(MARGIN);
                const int by = Scale(HEADER_H) + Scale(7);
                const int bh = Scale(26);

                AddButton(w, L"Open folder\x2026", bx, by, Scale(112), bh, ID_OPEN);
                AddButton(w, L"Save", bx + Scale(118), by, Scale(70), bh, ID_SAVE);
                AddButton(w, L"Save as\x2026", bx + Scale(192), by, Scale(84), bh, ID_SAVEAS);
                AddButton(w, L"Reload", bx + Scale(280), by, Scale(70), bh, ID_RELOAD);

                g_search = Add(w, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                               bx + Scale(370), by, Scale(200), Scale(24), ID_SEARCH);
                AddButton(w, L"Find", bx + Scale(576), by, Scale(60), bh, ID_SEARCH_GO);
                g_themeButton = AddButton(w, L"Theme: Light", bx + Scale(650), by,
                                          Scale(90), bh, ID_THEME);

                g_tabs = CreateWindowExW(0, WC_TABCONTROLW, nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                         TCS_OWNERDRAWFIXED | TCS_HOTTRACK,
                                         0, 0, 0, 0, w, (HMENU)(INT_PTR)ID_TABS,
                                         GetModuleHandleW(nullptr), nullptr);
                SendMessageW(g_tabs, WM_SETFONT, (WPARAM)g_font, TRUE);
                StyleControl(g_tabs);

                g_page = CreateWindowExW(WS_EX_CONTROLPARENT, L"MangosConfPage", nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
                                         0, 0, 0, 0, w, (HMENU)(INT_PTR)ID_PAGE,
                                         GetModuleHandleW(nullptr), nullptr);

                ApplyTheme();
                return 0;
            }

            case WM_SIZE:
            {
                // The window has no resizable frame, so every WM_SIZE after the
                // first carries the size already laid out -- restoring from the
                // taskbar is the usual one. Laying out again walks all three
                // hundred controls back to where they stand, and the screen
                // crawls through it; the restore repaints the client area on its
                // own.
                static int laidOutCx = -1;
                static int laidOutCy = -1;
                const int cx = LOWORD(lp);
                const int cy = HIWORD(lp);
                if (wp == SIZE_MINIMIZED || (cx == laidOutCx && cy == laidOutCy))
                {
                    return 0;
                }
                laidOutCx = cx;
                laidOutCy = cy;

                LayoutChildren();

                RECT rc{};
                GetClientRect(w, &rc);
                RECT band{ 0, 0, rc.right, Scale(HEADER_H) + Scale(TOOLBAR_H) };
                InvalidateRect(w, &band, TRUE);
                RECT strip{ 0, rc.bottom - Scale(STATUS_H), rc.right, rc.bottom };
                InvalidateRect(w, &strip, TRUE);
                return 0;
            }

            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(w, &ps);
                RECT rc{};
                GetClientRect(w, &rc);

                RECT below{ 0, Scale(HEADER_H), rc.right, Scale(HEADER_H) + Scale(TOOLBAR_H) };
                FillRect(dc, &below, g_chromeBrush ? g_chromeBrush
                                                   : GetSysColorBrush(COLOR_BTNFACE));
                PaintHeader(dc, rc);
                PaintStatus(dc, rc);

                EndPaint(w, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
            {
                RECT rc{};
                GetClientRect(w, &rc);
                RECT rest{ 0, Scale(HEADER_H), rc.right, rc.bottom - Scale(STATUS_H) };
                FillRect((HDC)wp, &rest, g_chromeBrush ? g_chromeBrush
                                                       : GetSysColorBrush(COLOR_BTNFACE));
                return 1;
            }

            case WM_DRAWITEM:
            {
                const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)lp;
                if (di->CtlType == ODT_TAB)
                {
                    DrawTab(di);
                    return TRUE;
                }
                if (di->CtlType == ODT_BUTTON)
                {
                    DrawButton(di);
                    return TRUE;
                }
                break;
            }

            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
                return ColorFor((HDC)wp, (HWND)lp, true);

            case WM_CTLCOLORSTATIC:
                // Search sits on the chrome strip, not the page.
                SetBkColor((HDC)wp, T().chromeBg);
                SetTextColor((HDC)wp, T().chromeText);
                return (LRESULT)g_chromeBrush;

            case WM_NOTIFY:
            {
                LPNMHDR hdr = (LPNMHDR)lp;
                if (hdr->idFrom == ID_TABS && hdr->code == TCN_SELCHANGING)
                {
                    Harvest();
                    return 0;
                }
                if (hdr->idFrom == ID_TABS && hdr->code == TCN_SELCHANGE)
                {
                    const int sel = (int)SendMessageW(g_tabs, TCM_GETCURSEL, 0, 0);
                    TCITEMW item{};
                    item.mask = TCIF_PARAM;
                    if (SendMessageW(g_tabs, TCM_GETITEMW, sel, (LPARAM)&item))
                    {
                        BuildPage((size_t)item.lParam);
                    }
                    return 0;
                }
                break;
            }

            case WM_COMMAND:
            {
                switch (LOWORD(wp))
                {
                    case ID_OPEN:
                        if (ConfirmDiscard())
                        {
                            OnOpenFolder();
                        }
                        return 0;

                    case ID_SAVE:
                        if (g_loaded)
                        {
                            DoSave(g_savePath);
                        }
                        return 0;

                    case ID_SAVEAS:
                    {
                        if (!g_loaded)
                        {
                            return 0;
                        }
                        std::wstring path = g_savePath;
                        if (SaveAsDialog(w, path))
                        {
                            DoSave(path);
                        }
                        return 0;
                    }

                    case ID_RELOAD:
                        if (g_loaded && ConfirmDiscard())
                        {
                            // Prefer the live save target when it already exists
                            // (opened a .dist, saved mangosd.conf -- reload that).
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
                                SetStatus(L"Nothing on disk to reload");
                            }
                            else
                            {
                                OpenPath(path);
                            }
                        }
                        return 0;

                    case ID_SEARCH_GO:
                        OnSearch();
                        return 0;

                    case ID_THEME:
                        CycleTheme();
                        return 0;

                    case ID_SEARCH:
                        if (HIWORD(wp) == EN_CHANGE)
                        {
                            // no live search -- wait for Find
                        }
                        break;

                    default:
                        break;
                }
                if (HIWORD(wp) == EN_SETFOCUS && LOWORD(wp) == ID_SEARCH)
                {
                    // Enter in the search box is handled below via IsDialogMessage.
                }
                break;
            }

            case WM_CLOSE:
                if (!ConfirmDiscard())
                {
                    return 0;
                }
                DestroyWindow(w);
                return 0;

            case WM_DESTROY:
                DestroyPage();
                if (g_pageBrush)
                {
                    DeleteObject(g_pageBrush);
                    g_pageBrush = nullptr;
                }
                if (g_fieldBrush)
                {
                    DeleteObject(g_fieldBrush);
                    g_fieldBrush = nullptr;
                }
                if (g_chromeBrush)
                {
                    DeleteObject(g_chromeBrush);
                    g_chromeBrush = nullptr;
                }
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
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    if (HDC screen = GetDC(nullptr))
    {
        g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
        ReleaseDC(nullptr, screen);
    }

    g_font = MakeFont(0, 0);
    g_bold = MakeFont(FW_SEMIBOLD, 0);
    g_title = MakeFont(FW_SEMIBOLD, 5);
    g_small = MakeFont(0, -1);

    WNDCLASSEXW page{};
    page.cbSize = sizeof(page);
    page.lpfnWndProc = PageProc;
    page.hInstance = instance;
    page.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    page.lpszClassName = L"MangosConfPage";
    RegisterClassExW(&page);

    WNDCLASSEXW main{};
    main.cbSize = sizeof(main);
    main.lpfnWndProc = MainProc;
    main.hInstance = instance;
    main.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    main.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    main.hIconSm = main.hIcon;
    main.lpszClassName = L"MangosConfEditor";
    RegisterClassExW(&main);

    // Fixed at the work area, with no thick frame and no maximise box: three
    // hundred settings want the whole screen anyway, and a size that cannot
    // change is a layout that cannot be caught half-drawn.
    RECT work{ 0, 0, Scale(780), Scale(660) };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    HWND w = CreateWindowExW(0, L"MangosConfEditor", L"MaNGOS Config Editor",
                             (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX))
                                 | WS_CLIPCHILDREN,
                             work.left, work.top,
                             work.right - work.left, work.bottom - work.top,
                             nullptr, nullptr, instance, nullptr);
    if (!w)
    {
        return 1;
    }

    ShowWindow(w, (show == SW_SHOWDEFAULT || show == SW_SHOWMAXIMIZED) ? SW_SHOW : show);
    UpdateWindow(w);

    // Command line wins; otherwise load a conf sitting next to the exe (or in
    // the parent folder when the editor is installed under tools/).
    std::wstring path;
    if (commandLine && *commandLine)
    {
        // Trailing blanks before the quotes come off first. A path that keeps one
        // still names the directory -- the filesystem trims it -- but joining a
        // file name onto it puts the blank in the middle, where it names nothing.
        std::wstring arg = commandLine;
        const size_t first = arg.find_first_not_of(L" \t\"");
        const size_t last = arg.find_last_not_of(L" \t\"");
        arg = (first == std::wstring::npos) ? std::wstring()
                                            : arg.substr(first, last - first + 1);
        const DWORD attributes = GetFileAttributesW(arg.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES)
        {
            path = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? FindConf(arg) : arg;
        }

        if (path.empty())
        {
            MessageBoxW(w, (L"Nothing to open in:\n" + arg).c_str(),
                        L"MaNGOS Config Editor", MB_OK | MB_ICONINFORMATION);
        }
    }
    else
    {
        path = FindConfBesideExe();
    }

    if (!path.empty())
    {
        OpenPath(path);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
            g_search && GetFocus() == g_search)
        {
            OnSearch();
            continue;
        }
        if (!IsDialogMessageW(w, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
