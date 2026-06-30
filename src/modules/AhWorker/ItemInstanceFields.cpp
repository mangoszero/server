#include "ItemInstanceFields.h"
#include <cstdlib>
#include <vector>

namespace
{
    // 1.12 field word indices (UpdateFields.h, OBJECT_END = 6):
    const size_t IIF_SPELL_CHARGES = 16;   // OBJECT_END + 0x0A
    const size_t IIF_STACK_COUNT   = 14;   // OBJECT_END + 0x08
    const size_t IIF_ENCHANT_ID    = 22;   // OBJECT_END + 0x10 + PERM(0)*3 + 0
    const size_t IIF_PROPERTY_SEED = 43;   // OBJECT_END + 0x25
    const size_t IIF_RANDOM_PROP   = 44;   // OBJECT_END + 0x26
    const size_t IIF_MIN_WORDS     = IIF_RANDOM_PROP + 1;   // need word 44 present
}

namespace AhItemBlob
{
    ItemInstanceFields Decode(const std::string& dataBlob)
    {
        ItemInstanceFields f;
        f.enchantId = 0u; f.suffixFactor = 0u; f.randomPropId = 0;
        f.charges = 0; f.stackCount = 0; f.valid = false;

        // Split on single spaces (Object::LoadValues uses StrSplit(data, " ")).
        std::vector<std::string> words;
        words.reserve(64);
        size_t start = 0;
        while (start <= dataBlob.size())
        {
            size_t sp = dataBlob.find(' ', start);
            if (sp == std::string::npos)
            {
                if (start < dataBlob.size())
                {
                    words.push_back(dataBlob.substr(start));
                }
                break;
            }
            words.push_back(dataBlob.substr(start, sp - start));
            start = sp + 1;
        }

        if (words.size() < IIF_MIN_WORDS)
        {
            return f;   // valid stays false
        }

        // atol matches Object::LoadValues; values are written as decimal uint32
        // (signed fields round-trip through the uint32 slot).
        f.stackCount   = static_cast<uint32>(strtoul(words[IIF_STACK_COUNT].c_str(),   NULL, 10));
        f.charges      = static_cast<int32>( strtol( words[IIF_SPELL_CHARGES].c_str(), NULL, 10));
        f.enchantId    = static_cast<uint32>(strtoul(words[IIF_ENCHANT_ID].c_str(),    NULL, 10));
        f.suffixFactor = static_cast<uint32>(strtoul(words[IIF_PROPERTY_SEED].c_str(), NULL, 10));
        f.randomPropId = static_cast<int32>( strtol( words[IIF_RANDOM_PROP].c_str(),   NULL, 10));
        f.valid = true;
        return f;
    }
}
