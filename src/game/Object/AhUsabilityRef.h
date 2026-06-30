#ifndef MANGOS_AH_USABILITY_REF_H
#define MANGOS_AH_USABILITY_REF_H

#include "Common.h"

/// Self-contained usability verdict. Deliberately NOT the game InventoryResult
/// enum: this header is compiled standalone into the Task-8 difftest, which must
/// NOT pull in Object/Item.h. Player::CanUseItem maps each value 1:1 to the exact
/// InventoryResult / EQUIP_ERR_* (D1 -- no code collapse); the difftest only
/// needs == AHUSE_OK. A static_assert in Player.cpp guards the mapping.
enum AhUseResult : uint8
{
    AHUSE_OK = 0,                ///< -> EQUIP_ERR_OK
    AHUSE_NEVER_USE,             ///< -> EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM (class/race)
    AHUSE_NO_PROFICIENCY,        ///< -> EQUIP_ERR_NO_REQUIRED_PROFICIENCY (skill/spell absent)
    AHUSE_CANT_EQUIP_SKILL,      ///< -> EQUIP_ERR_CANT_EQUIP_SKILL (skill rank too low)
    AHUSE_CANT_EQUIP_RANK,       ///< -> EQUIP_ERR_CANT_EQUIP_RANK (honor)
    AHUSE_CANT_EQUIP_LEVEL,      ///< -> EQUIP_ERR_CANT_EQUIP_LEVEL_I (level)
    AHUSE_CANT_EQUIP_REPUTATION  ///< -> EQUIP_ERR_CANT_EQUIP_REPUTATION
};

/// Item-side requirement columns the usability predicate reads. Mirrors the
/// worker's ItemUsabilityReq one-for-one so the diff test compares like-for-like.
struct AhRefItem
{
    uint32 itemClass;            ///< ItemPrototype::Class
    uint32 allowableClass;       ///< ItemPrototype::AllowableClass
    uint32 allowableRace;        ///< ItemPrototype::AllowableRace
    uint32 requiredLevel;        ///< ItemPrototype::RequiredLevel
    uint32 itemId;               ///< ItemPrototype::ItemId (mount override key)
    uint32 requiredSkill;        ///< ItemPrototype::RequiredSkill
    uint32 requiredSkillRank;    ///< ItemPrototype::RequiredSkillRank
    uint32 requiredSpell;        ///< ItemPrototype::RequiredSpell
    uint32 requiredHonorRank;    ///< ItemPrototype::RequiredHonorRank
    uint32 requiredRepFaction;   ///< ItemPrototype::RequiredReputationFaction (0 = skip)
    uint32 requiredRepRank;      ///< ItemPrototype::RequiredReputationRank
    uint32 itemProficiencySkill; ///< Item::GetSkill() result (0 = skip)
};

namespace AhUsabilityRef
{
    /// The production usability predicate (the prototype-level CanUseItem
    /// branches + the Item* proficiency/reputation extras), minus the Eluna
    /// hook. Player::CanUseItem delegates here so the test and production share
    /// one source. Returns the per-branch AhUseResult (mapped 1:1 to the exact
    /// InventoryResult in Player.cpp, D1) so callers that forward a specific code
    /// keep their behavior; the honor branch is gated on directAction (D2).
    /// Accessors are passed so the same code serves the live Player (this-bound
    /// thunks) and the diff harness (array-backed C functions).
    AhUseResult Evaluate(uint32 classMask, uint32 raceMask, uint32 level,
                         uint32 honorRank, bool directAction,
                         uint32 minMountLevel, uint32 minEpicMountLevel,
                         const AhRefItem& item,
                         uint16 (*skillRank)(void* ctx, uint32 skillId),
                         bool   (*hasSpell)(void* ctx, uint32 spellId),
                         uint8  (*repRank)(void* ctx, uint32 factionId),
                         void* ctx);
}

#endif // MANGOS_AH_USABILITY_REF_H
