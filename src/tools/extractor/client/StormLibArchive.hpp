#pragma once

// Real client data, backed by StormLib. Archives are opened lowest-priority first
// and searched last-first, so the patch MPQs override the base ones exactly as the
// client resolves them.

#include "IMpqArchive.hpp"

#include <string>
#include <vector>

namespace world::terrain
{
    class StormLibArchive : public IMpqArchive
    {
    public:
        StormLibArchive() = default;
        ~StormLibArchive() override;

        StormLibArchive(const StormLibArchive&) = delete;
        StormLibArchive& operator=(const StormLibArchive&) = delete;

        bool AddArchive(const std::string& mpqPath);

        // Both lists are lowest-priority first and the locale ones are opened last.
        // Which archives exist is the largest difference between client versions, so
        // the names are passed in rather than known here; {locale} expands to `locale`
        // and the locale paths are relative to the locale directory. Missing names are
        // skipped, not an error.
        int OpenClientData(const std::string& dataDir,
                           const std::vector<std::string>& archives,
                           const std::vector<std::string>& localeArchives,
                           const std::string& locale);

        bool Read(const std::string& path, std::vector<uint8_t>& out) override;
        bool Contains(const std::string& path) const override;

        std::vector<std::string> FindFiles(const std::string& pattern) const;

        size_t ArchiveCount() const { return m_handles.size(); }

    private:
        std::vector<void*> m_handles;
    };

    // The 3.3.5a archive chain, lowest priority first.
    const std::vector<std::string>& ClientArchives112();
    const std::vector<std::string>& ClientLocaleArchives112();
}
