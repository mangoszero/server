#pragma once

// The one thing the client-data parsers need: raw bytes by in-MPQ path. Everything
// above this line is pure byte-pushing, so it is unit-testable against MemoryArchive
// with no StormLib and no client install.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace world::terrain
{
    class IMpqArchive
    {
    public:
        virtual ~IMpqArchive() = default;

        virtual bool Read(const std::string& path, std::vector<uint8_t>& out) = 0;
        virtual bool Contains(const std::string& path) const = 0;
    };

    class MemoryArchive : public IMpqArchive
    {
    public:
        void Put(const std::string& path, std::vector<uint8_t> bytes)
        {
            m_files[Normalize(path)] = std::move(bytes);
        }

        bool Read(const std::string& path, std::vector<uint8_t>& out) override
        {
            auto it = m_files.find(Normalize(path));
            if (it == m_files.end())
            {
                return false;
            }
            out = it->second;
            return true;
        }

        bool Contains(const std::string& path) const override
        {
            return m_files.count(Normalize(path)) != 0;
        }

    private:
        static std::string Normalize(const std::string& p)
        {
            std::string s;
            s.reserve(p.size());
            for (char c : p)
            {
                if (c == '/')
                {
                    c = '\\';
                }
                if (c >= 'A' && c <= 'Z')
                {
                    c = static_cast<char>(c - 'A' + 'a');
                }
                s.push_back(c);
            }
            return s;
        }

        std::unordered_map<std::string, std::vector<uint8_t>> m_files;
    };
}
