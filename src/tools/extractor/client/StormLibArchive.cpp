#include <string>
#include <vector>
#include "StormLibArchive.hpp"

#include <StormLib.h>

#include <cstdio>
#include <unordered_set>

namespace world::terrain
{
    namespace
    {
        std::string ExpandLocale(std::string s, const std::string& locale)
        {
            const std::string token = "{locale}";
            for (size_t at = s.find(token); at != std::string::npos;
                 at = s.find(token, at + locale.size()))
            {
                s.replace(at, token.size(), locale);
            }
            return s;
        }
    }

    // Lowest priority first: Read walks the handles in reverse, so the last archive
    // opened is the first searched. Verified against StormLib's own patch chain
    // (SFileOpenPatchArchive) over every DBC and WDT plus a sample of ADTs -- 1047
    // files, byte-identical.
    const std::vector<std::string>& ClientArchives112()
    {
        // Checked against a real 1.12.1 install: 1.12 ships ONE ARCHIVE PER ASSET KIND
        // rather than the expansion-stacked set 3.3.5a uses -- there is no common.MPQ, no
        // expansion.MPQ and no lichking.MPQ, and dbc/terrain/model/wmo are separate files.
        // Lowest priority first, so the numbered patches come last and win.
        //
        // texture, sound, speech, fonts, interface and backup are deliberately absent:
        // none of them holds a DBC, WDT, ADT, WMO or M2, so opening them would cost
        // handles and buy nothing. Same reasoning as the 3.3.5a speech archives.
        static const std::vector<std::string> archives = {
            "base.MPQ",  "dbc.MPQ",   "misc.MPQ",   "model.MPQ",
            "terrain.MPQ", "wmo.MPQ",
            "patch.MPQ", "patch-2.MPQ",
        };
        return archives;
    }

    const std::vector<std::string>& ClientLocaleArchives112()
    {
        // Same ordering rule as 3.3.5a: numbered locale patches last, or Map.dbc is read
        // from an older archive that parses perfectly and is simply missing rows.
        //
        // A locale-merged 1.12 install has no Data/<locale> directory at all and every one
        // of these is simply absent; OpenClientData counts what opened and does not care.
        // The DBCs are then in the root dbc.MPQ.
        static const std::vector<std::string> archives = {
            "base-{locale}.MPQ",
            "locale-{locale}.MPQ",
            "patch-{locale}.MPQ",
            "patch-{locale}-2.MPQ",
        };
        return archives;
    }

    StormLibArchive::~StormLibArchive()
    {
        for (void* h : m_handles)
        {
            if (h)
            {
                SFileCloseArchive(h);
            }
        }
    }

    bool StormLibArchive::AddArchive(const std::string& mpqPath)
    {
        HANDLE h = nullptr;
        if (!SFileOpenArchive(mpqPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &h))
        {
            return false;
        }
        m_handles.push_back(h);
        return true;
    }

    int StormLibArchive::OpenClientData(const std::string& dataDir,
                                        const std::vector<std::string>& archives,
                                        const std::vector<std::string>& localeArchives,
                                        const std::string& locale)
    {
        int opened = 0;
        for (const std::string& rel : archives)
        {
            if (AddArchive(dataDir + "/" + ExpandLocale(rel, locale)))
            {
                ++opened;
            }
        }

        const std::string localeDir = dataDir + "/" + locale;
        for (const std::string& rel : localeArchives)
        {
            if (AddArchive(localeDir + "/" + ExpandLocale(rel, locale)))
            {
                ++opened;
            }
        }
        return opened;
    }

    bool StormLibArchive::Read(const std::string& path, std::vector<uint8_t>& out)
    {
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            HANDLE hFile = nullptr;
            if (!SFileOpenFileEx(*it, path.c_str(), 0, &hFile))
            {
                continue;
            }

            DWORD high = 0;
            const DWORD size = SFileGetFileSize(hFile, &high);
            if (size == SFILE_INVALID_SIZE)
            {
                SFileCloseFile(hFile);
                continue;
            }

            out.resize(size);
            DWORD got = 0;
            if (size > 0)
            {
                SFileReadFile(hFile, out.data(), size, &got, nullptr);
            }
            SFileCloseFile(hFile);
            if (got == size)
            {
                return true;
            }
            out.clear();
        }
        return false;
    }

    bool StormLibArchive::Contains(const std::string& path) const
    {
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            if (SFileHasFile(*it, const_cast<char*>(path.c_str())))
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> StormLibArchive::FindFiles(const std::string& pattern) const
    {
        std::vector<std::string> result;
        if (m_handles.empty() || pattern.empty())
        {
            return result;
        }

        std::unordered_set<std::string> seen;
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            SFILE_FIND_DATA findData{};
            HANDLE hFind = SFileFindFirstFile(*it, pattern.c_str(), &findData, nullptr);
            if (!hFind)
            {
                continue;
            }
            do
            {
                std::string name(findData.cFileName);
                if (seen.insert(name).second)
                {
                    result.push_back(std::move(name));
                }
            } while (SFileFindNextFile(hFind, &findData));
            SFileFindClose(hFind);
        }
        return result;
    }
}
