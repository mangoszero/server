/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 */

#ifndef AH_IPC_BROWSE_MESSAGES_H
#define AH_IPC_BROWSE_MESSAGES_H

#include "Common.h"
#include "Utilities/ByteBuffer.h"
#include <string>
#include <vector>

/**
 * @file BrowseMessages.h
 * @brief Wire types for the SP-1 AH read/browse IPC path.
 *
 *   BrowseQuery  -> IPC_BROWSE_QUERY  (0x1030)  mangosd  -> ah-service
 *   BrowseResult -> IPC_BROWSE_RESULT (0x1031)  ah-service -> mangosd
 *
 * Both bodies are variable length, so they register IPC_RULE_MAXLEN rules in
 * IpcExpectedBodySize, NOT a fixed WIRE_SIZE.
 *
 * Strings are LENGTH-PREFIXED (uint16 len + bytes): ByteBuffer::operator>>(
 * std::string) consumes to the end of the buffer and never throws, so a
 * NUL-terminated convention cannot be safely followed by more fields.
 *
 * GUIDs travel as raw low-GUID uint32; mangosd wraps them in
 * ObjectGuid(HIGHGUID_PLAYER, low) when assembling the SMSG.
 */

enum BrowseKind : uint8
{
    BROWSE_LIST     = 0,  ///< CMSG_AUCTION_LIST_ITEMS (filters + pagination)
    BROWSE_OWNER    = 1,  ///< AUCTION_LIST_OWNER_ITEMS (no filters/pagination)
    BROWSE_BIDDER   = 2,  ///< AUCTION_LIST_BIDDER_ITEMS (+ client outbid id list)
    BROWSE_KIND_MAX = 3   ///< exclusive upper bound for validation
};

/// One learned skill and its current rank.
struct SkillRank
{
    uint16 skillId;
    uint16 rank;
};

/// One reputation standing (faction -> ReputationRank 0..7).
struct RepStanding
{
    uint16 factionId;
    uint8  rank;
};

/// Length-prefixed string codec helpers (uint16 length cap = 8192).
namespace BrowseWire
{
    const uint16 MAX_STRING = 8192u;

    inline void PutString(ByteBuffer& buf, const std::string& s)
    {
        uint16 n = static_cast<uint16>(s.size() > BrowseWire::MAX_STRING
                                       ? BrowseWire::MAX_STRING : s.size());
        buf << n;
        if (n > 0)
        {
            buf.append(s.data(), n);
        }
    }

    inline bool GetString(ByteBuffer& buf, std::string& out)
    {
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        uint16 n = 0;
        buf >> n;
        if (n > BrowseWire::MAX_STRING || buf.rpos() + n > buf.size())
        {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(buf.contents()) + buf.rpos(), n);
        buf.read_skip(n);
        return true;
    }
}

/// Flat snapshot of the browsing player's usability inputs.
struct PlayerProfile
{
    uint8  classId;
    uint8  raceId;
    uint8  level;
    uint8  honorRank;                    ///< GetHonorHighestRankInfo().rank (0..18)
    std::vector<SkillRank>   skills;     ///< (skillId, GetSkillValue)
    std::vector<uint32>      knownSpells;///< HasSpell set
    std::vector<RepStanding> reps;       ///< faction -> ReputationRank

    void Encode(ByteBuffer& buf) const
    {
        buf << classId << raceId << level << honorRank;
        buf << uint16(skills.size());
        for (size_t i = 0; i < skills.size(); ++i)
        {
            buf << skills[i].skillId << skills[i].rank;
        }
        buf << uint16(knownSpells.size());
        for (size_t i = 0; i < knownSpells.size(); ++i)
        {
            buf << knownSpells[i];
        }
        buf << uint16(reps.size());
        for (size_t i = 0; i < reps.size(); ++i)
        {
            buf << reps[i].factionId << reps[i].rank;
        }
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + 4 > buf.size())
        {
            return false;
        }
        buf >> classId >> raceId >> level >> honorRank;
        uint16 n = 0;
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        buf >> n;
        skills.clear();
        for (uint16 i = 0; i < n; ++i)
        {
            if (buf.rpos() + 4 > buf.size())
            {
                return false;
            }
            SkillRank sr;
            buf >> sr.skillId >> sr.rank;
            skills.push_back(sr);
        }
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        buf >> n;
        knownSpells.clear();
        for (uint16 i = 0; i < n; ++i)
        {
            if (buf.rpos() + 4 > buf.size())
            {
                return false;
            }
            uint32 sp;
            buf >> sp;
            knownSpells.push_back(sp);
        }
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        buf >> n;
        reps.clear();
        for (uint16 i = 0; i < n; ++i)
        {
            if (buf.rpos() + 3 > buf.size())
            {
                return false;
            }
            RepStanding rs;
            buf >> rs.factionId >> rs.rank;
            reps.push_back(rs);
        }
        return true;
    }
};

/// IPC_BROWSE_QUERY (0x1030) body. Variable length.
struct BrowseQuery
{
    // Fixed prefix (before the first length-prefixed field, searchedName):
    //   8 + 1+1+1 + 4+4+4+4 + 1+1+1+1 + 4 + 1 + 4 + 4+4 = 48 bytes.
    // (D8) The COMPLETE struct -- including the two mount-config fields -- is
    // defined here ONCE so no later task mutates the wire layout. Decode guards
    // on this single named constant; keep it in sync with the field list above.
    static const size_t FIXED_PREFIX = 48u;
    static const size_t MAX_WIRE     = 16384u;

    uint64 queryId;
    uint8  kind;            ///< BrowseKind
    uint8  house;           ///< 0=ALLIANCE,1=HORDE,2=NEUTRAL (ignored if allHouses)
    uint8  allHouses;       ///< 1 = AllowTwoSide.Interaction.Auction -> neutral-only scope
    uint32 itemClass;       ///< 0xFFFFFFFF = any
    uint32 itemSubClass;    ///< 0xFFFFFFFF = any
    uint32 inventoryType;   ///< 0xFFFFFFFF = any
    uint32 quality;         ///< 0xFFFFFFFF = any (else proto.Quality >= quality)
    uint8  levelmin;        ///< 0 = no min
    uint8  levelmax;        ///< 0 = no max
    uint8  usable;          ///< 0 = no usable filter
    uint8  deferEluna;      ///< 1 = ENABLE_ELUNA build AND usable: defer OnCanUseItem to mangosd
    uint32 listfrom;        ///< page offset (LIST only)
    int8   localeIndex;     ///< V3: LocaleConstant (0=enUS/default=no overlay; 1..MAX_LOCALE-1 = name_locN)
    uint32 requesterGuidLow;///< OWNER/BIDDER: the requesting character low GUID
    uint32 minMountLevel;       ///< CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL (mount override; populated Task 6/11)
    uint32 minEpicMountLevel;   ///< CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL (populated Task 6/11)
    std::string searchedName;             ///< lower-cased UTF-8 name fragment ("" = any)
    std::vector<uint32> outbidIds;        ///< BIDDER only: client outbid auction ids (client order)
    std::vector<uint32> knownRecipeCastSpells; ///< derived spellid_1 set the player already knows (C2b)
    PlayerProfile profile;

    void Encode(ByteBuffer& buf) const
    {
        buf << queryId << kind << house << allHouses
            << itemClass << itemSubClass << inventoryType << quality
            << levelmin << levelmax << usable << deferEluna
            << listfrom << localeIndex << requesterGuidLow
            << minMountLevel << minEpicMountLevel;
        BrowseWire::PutString(buf, searchedName);
        buf << uint16(outbidIds.size());
        for (size_t i = 0; i < outbidIds.size(); ++i)
        {
            buf << outbidIds[i];
        }
        buf << uint16(knownRecipeCastSpells.size());
        for (size_t i = 0; i < knownRecipeCastSpells.size(); ++i)
        {
            buf << knownRecipeCastSpells[i];
        }
        profile.Encode(buf);
    }

    bool Decode(ByteBuffer& buf)
    {
        // M1/D8: guard the single named FIXED_PREFIX (48) so a struct change is a
        // one-line constant edit, never a magic-number hunt across tasks.
        if (buf.rpos() + BrowseQuery::FIXED_PREFIX > buf.size())
        {
            return false;
        }
        buf >> queryId >> kind >> house >> allHouses
            >> itemClass >> itemSubClass >> inventoryType >> quality
            >> levelmin >> levelmax >> usable >> deferEluna
            >> listfrom >> localeIndex >> requesterGuidLow
            >> minMountLevel >> minEpicMountLevel;
        if (kind >= static_cast<uint8>(BROWSE_KIND_MAX))
        {
            return false;
        }
        if (!BrowseWire::GetString(buf, searchedName))
        {
            return false;
        }
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        uint16 n = 0;
        buf >> n;
        outbidIds.clear();
        for (uint16 i = 0; i < n; ++i)
        {
            if (buf.rpos() + 4 > buf.size())
            {
                return false;
            }
            uint32 id;
            buf >> id;
            outbidIds.push_back(id);
        }
        if (buf.rpos() + 2 > buf.size())
        {
            return false;
        }
        buf >> n;
        knownRecipeCastSpells.clear();
        for (uint16 i = 0; i < n; ++i)
        {
            if (buf.rpos() + 4 > buf.size())
            {
                return false;
            }
            uint32 sp;
            buf >> sp;
            knownRecipeCastSpells.push_back(sp);
        }
        if (!profile.Decode(buf))
        {
            return false;
        }
        // V5/I7: reject trailing bytes -- a well-formed body consumes exactly.
        return buf.rpos() == buf.size();
    }
};

/// One auction row, fields in BuildAuctionInfo order (GUIDs as raw low uint32).
struct BrowseEntry
{
    uint32 id;
    uint32 itemEntry;
    uint32 enchantId;       ///< GetEnchantmentId(PERM_ENCHANTMENT_SLOT)
    uint32 randomPropId;    ///< auction.item_randompropertyid (written as uint32, as live)
    uint32 suffixFactor;    ///< GetItemSuffixFactor()
    uint32 count;           ///< GetCount()
    int32  charges;         ///< GetSpellCharges() (signed; live writes it as uint32)
    uint32 ownerGuidLow;
    uint32 startbid;
    uint32 outbid;          ///< bid ? GetAuctionOutBid() : 0
    uint32 buyout;
    uint32 timeLeftMs;
    uint32 bidderGuidLow;
    uint32 curBid;

    void Encode(ByteBuffer& buf) const
    {
        buf << id << itemEntry << enchantId << randomPropId << suffixFactor
            << count << charges << ownerGuidLow << startbid << outbid
            << buyout << timeLeftMs << bidderGuidLow << curBid;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + 14 * 4 > buf.size())
        {
            return false;
        }
        buf >> id >> itemEntry >> enchantId >> randomPropId >> suffixFactor
            >> count >> charges >> ownerGuidLow >> startbid >> outbid
            >> buyout >> timeLeftMs >> bidderGuidLow >> curBid;
        return true;
    }
};

/// IPC_BROWSE_RESULT (0x1031) body. Variable length.
struct BrowseResult
{
    static const size_t MAX_WIRE = 65536u;
    static const uint32 MAX_ENTRIES = 1024u;   ///< hard ceiling (>= Eluna cap 1000)

    uint64 queryId;
    uint8  kind;
    uint8  elunaPending; ///< 1 = un-paginated set; mangosd runs OnCanUseItem + paginate
    uint8  tooMany;      ///< 1 = worker declined (queue full / oversize failsafe / over-cap deferred-Eluna); no entries; mangosd sends "AH unavailable"
    uint32 totalcount;   ///< total matches (post-filter; pre-pagination)
    std::vector<BrowseEntry> entries;

    size_t Count() const { return entries.size(); }

    void Encode(ByteBuffer& buf) const
    {
        buf << queryId << kind << elunaPending << tooMany << totalcount;
        buf << uint32(entries.size());
        for (size_t i = 0; i < entries.size(); ++i)
        {
            entries[i].Encode(buf);
        }
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + 8 + 1 + 1 + 1 + 4 + 4 > buf.size())
        {
            return false;
        }
        buf >> queryId >> kind >> elunaPending >> tooMany >> totalcount;
        if (kind >= static_cast<uint8>(BROWSE_KIND_MAX))
        {
            return false;
        }
        uint32 n = 0;
        buf >> n;
        if (n > MAX_ENTRIES)
        {
            return false;
        }
        entries.clear();
        for (uint32 i = 0; i < n; ++i)
        {
            BrowseEntry e;
            if (!e.Decode(buf))
            {
                return false;   // declared count exceeds available bytes (I7)
            }
            entries.push_back(e);
        }
        // F4: fail closed on semantically-invalid flag combinations so a corrupt
        // frame is treated as "unavailable" rather than reinterpreted as a valid
        // over-serve. The flags are booleans; a tooMany (declined) result is
        // mutually exclusive with elunaPending and carries no entries.
        if (elunaPending > 1u || tooMany > 1u)
        {
            return false;
        }
        if (tooMany != 0u && (elunaPending != 0u || !entries.empty()))
        {
            return false;
        }
        // V5/I7: reject trailing bytes (the declared count must consume the body
        // exactly). BrowseEntry::Decode does NOT check this -- it reads one entry
        // mid-buffer; the whole-body invariant belongs here.
        return buf.rpos() == buf.size();
    }
};

#endif // AH_IPC_BROWSE_MESSAGES_H
