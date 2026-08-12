#include "botpch.h"
#include "playerbot.h"
#include "ahbot/AhBot.h"
#include "LootAction.h"

#include "LootObjectStack.h"
#include "PlayerbotAIConfig.h"

#include "RandomPlayerbotMgr.h"
#include "strategy/values/ItemUsageValue.h"

using namespace ai;

bool LootAction::Execute(Event event)
{
    if (!AI_VALUE(bool, "has available loot"))
    {
        return false;
    }

    LootObject const& lootObject = AI_VALUE(LootObjectStack*, "available loot")->GetLoot(sPlayerbotAIConfig.lootDistance);
    context->GetValue<LootObject>("loot target")->Set(lootObject);
    return true;
}

enum ProfessionSpells
{
    ALCHEMY                      = 2259,
    BLACKSMITHING                = 2018,
    COOKING                      = 2550,
    ENCHANTING                   = 7411,
    ENGINEERING                  = 49383,
    FIRST_AID                    = 3273,
    FISHING                      = 7620,
    HERB_GATHERING               = 2366,
    INSCRIPTION                  = 45357,
    JEWELCRAFTING                = 25229,
    MINING                       = 2575,
    SKINNING                     = 8613,
    TAILORING                    = 3908
};

bool OpenLootAction::Execute(Event event)
{
    LootObject lootObject = AI_VALUE(LootObject, "loot target");
    LootResult result = DoLoot(lootObject);

    switch (result)
    {
        case LOOT_OK:
            AI_VALUE(LootObjectStack*, "available loot")->Remove(lootObject.guid);
            context->GetValue<LootObject>("loot target")->Set(LootObject());
            m_retryGuid = ObjectGuid();
            m_retryStarted = 0;
            return true;

        case LOOT_RETRY:
            // Retryable, but on a clock. Start it when the target changes; when it expires,
            // treat the target exactly as impossible. This is what stops a failure that
            // never clears -- a shapeshifted druid casting Mining, say -- from holding the
            // bot on the same object for the rest of its life.
            if (m_retryGuid != lootObject.guid)
            {
                m_retryGuid = lootObject.guid;
                m_retryStarted = time(0);
                return false;
            }

            if (time(0) - m_retryStarted <= (time_t)LOOT_RETRY_SECONDS)
            {
                return false;
            }

            // Fall through: out of patience, handle it as impossible.

        case LOOT_IMPOSSIBLE:
            // Let this one go, and stop it being offered again for a couple of minutes.
            //
            // Clearing the target alone would not help: the gather actions rescan and re-Add
            // the same object on the very next pass, the bot picks it again, and it is stuck
            // in the same place -- unable to loot and, because "can loot" stays true, unable
            // to follow its master either. Blacklisting is what actually breaks the loop.
            //
            // Deliberately NOT treated as success: nothing was looted, and reporting
            // otherwise would tell the engine this tick did useful work.
            AI_VALUE(LootObjectStack*, "available loot")->Blacklist(lootObject.guid);
            context->GetValue<LootObject>("loot target")->Set(LootObject());
            m_retryGuid = ObjectGuid();
            m_retryStarted = 0;
            return false;

        default:
            return false;
    }
}

OpenLootAction::LootResult OpenLootAction::DoLoot(LootObject& lootObject)
{
    if (lootObject.IsEmpty())
    {
        return LOOT_IMPOSSIBLE;
    }

    Creature* creature = ai->GetCreature(lootObject.guid);
    if (creature && bot->GetDistance(creature) > INTERACTION_DISTANCE)
    {
        return LOOT_RETRY;
    }

    if (creature && creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE))
    {
        bot->GetMotionMaster()->Clear();
        WorldPacket* const packet = new WorldPacket(CMSG_LOOT, 8);
        *packet << lootObject.guid;
        bot->GetSession()->QueuePacket(packet);
        return LOOT_OK;
    }

    if (creature)
    {
        SkillType skill = creature->GetCreatureInfo()->GetRequiredLootSkill();
        if (!CanOpenLock(skill, lootObject.reqSkillValue))
        {
            return LOOT_IMPOSSIBLE;
        }

        bot->GetMotionMaster()->Clear();
        switch (skill)
        {
            // A missing profession is permanent; a cast that merely failed is not.
            case SKILL_ENGINEERING:
                if (!bot->HasSkill(SKILL_ENGINEERING)) { return LOOT_IMPOSSIBLE; }
                return ai->CastSpell(ENGINEERING, creature) ? LOOT_OK : LOOT_RETRY;
            case SKILL_HERBALISM:
                if (!bot->HasSkill(SKILL_HERBALISM)) { return LOOT_IMPOSSIBLE; }
                return ai->CastSpell(32605, creature) ? LOOT_OK : LOOT_RETRY;
            case SKILL_MINING:
                if (!bot->HasSkill(SKILL_MINING)) { return LOOT_IMPOSSIBLE; }
                return ai->CastSpell(32606, creature) ? LOOT_OK : LOOT_RETRY;
            default:
                if (!bot->HasSkill(SKILL_SKINNING)) { return LOOT_IMPOSSIBLE; }
                return ai->CastSpell(SKINNING, creature) ? LOOT_OK : LOOT_RETRY;
        }
    }

    GameObject* go = ai->GetGameObject(lootObject.guid);
    if (go && bot->GetDistance(go) > INTERACTION_DISTANCE)
    {
        return LOOT_RETRY;
    }

    // Neither a creature nor a gameobject is here any more -- looted by someone else, or
    // despawned. Nothing will ever come of this target.
    if (!creature && !go)
    {
        return LOOT_IMPOSSIBLE;
    }

    bot->GetMotionMaster()->Clear();

    if (go && go->GetGoState() == GO_STATE_ACTIVE)
    {
        // Already open. That is a completed open, not a "try again" -- StoreLootAction works
        // from the response packet's guid, so clearing the AI target cannot disrupt it, while
        // keeping it selected needlessly suppresses following until release.
        if (bot->GetLootGuid() == lootObject.guid)
        {
            return LOOT_OK;
        }

        WorldPacket* const packet = new WorldPacket(CMSG_LOOT, 8);
        *packet << lootObject.guid;
        bot->GetSession()->QueuePacket(packet);
        return LOOT_OK;
    }

    if (bot->IsNonMeleeSpellCasted(false))
    {
        return LOOT_RETRY;
    }

    if (lootObject.skillId == SKILL_MINING)
    {
        if (!bot->HasSkill(SKILL_MINING)) { return LOOT_IMPOSSIBLE; }
        return ai->CastSpell(MINING, bot) ? LOOT_OK : LOOT_RETRY;
    }

    if (lootObject.skillId == SKILL_HERBALISM)
    {
        if (!bot->HasSkill(SKILL_HERBALISM)) { return LOOT_IMPOSSIBLE; }
        return ai->CastSpell(HERB_GATHERING, bot) ? LOOT_OK : LOOT_RETRY;
    }

    uint32 spellId = GetOpeningSpell(lootObject);
    if (!spellId)
    {
        return LOOT_IMPOSSIBLE;
    }

    return ai->CastSpell(spellId, bot) ? LOOT_OK : LOOT_RETRY;
}

uint32 OpenLootAction::GetOpeningSpell(LootObject& lootObject)
{
    GameObject* go = ai->GetGameObject(lootObject.guid);
    if (go && go->isSpawned())
    {
        return GetOpeningSpell(lootObject, go);
    }

    return 0;
}

uint32 OpenLootAction::GetOpeningSpell(LootObject& lootObject, GameObject* go)
{
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
        {
            continue;
        }

        if (spellId == MINING || spellId == HERB_GATHERING)
        {
            continue;
        }

        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (!pSpellInfo)
        {
            continue;
        }

        if (CanOpenLock(lootObject, pSpellInfo, go))
        {
            return spellId;
        }
    }

    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); spellId++)
    {
        if (spellId == MINING || spellId == HERB_GATHERING)
        {
            continue;
        }

        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (!pSpellInfo)
        {
            continue;
        }

        if (CanOpenLock(lootObject, pSpellInfo, go))
        {
            return spellId;
        }
    }

    return 0; //Spell 3365 = Opening?
}

bool OpenLootAction::CanOpenLock(LootObject& lootObject, const SpellEntry* pSpellInfo, GameObject* go)
{
    for (int effIndex = 0; effIndex < MAX_EFFECT_INDEX; effIndex++)
    {
        if (pSpellInfo->Effect[effIndex] != SPELL_EFFECT_OPEN_LOCK && pSpellInfo->Effect[effIndex] != SPELL_EFFECT_SKINNING)
        {
            return false;
        }

        uint32 lockId = go->GetGOInfo()->GetLockId();
        if (!lockId)
        {
            return false;
        }

        LockEntry const *lockInfo = sLockStore.LookupEntry(lockId);
        if (!lockInfo)
        {
            return false;
        }

        bool reqKey = false;                                    // some locks not have reqs

        for (int j = 0; j < 8; ++j)
        {
            switch (lockInfo->Type[j])
            {
                /**
                 * case LOCK_KEY_ITEM:
                 * return true;
                 */
                case LOCK_KEY_SKILL:
                {
                    if (uint32(pSpellInfo->EffectMiscValue[effIndex]) != lockInfo->Index[j])
                    {
                        continue;
                    }

                    uint32 skillId = SkillByLockType(LockType(lockInfo->Index[j]));
                    if (skillId == SKILL_NONE)
                    {
                        return true;
                    }

                    if (CanOpenLock(skillId, lockInfo->Skill[j]))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool OpenLootAction::CanOpenLock(uint32 skillId, uint32 reqSkillValue)
{
    uint32 skillValue = bot->GetSkillValue(skillId);
    return skillValue >= reqSkillValue || !reqSkillValue;
}

bool StoreLootAction::Execute(Event event)
{
    WorldPacket p(event.getPacket()); // (8+1+4+1+1+4+4+4+4+4+1)
    ObjectGuid guid;
    uint8 loot_type;
    uint32 gold;
    uint8 items;

    p.rpos(0);
    p >> guid;      // 8 corpse guid
    p >> loot_type; // 1 loot type
    p >> gold;      // 4 money on corpse
    p >> items;     // 1 number of items on corpse

    if (gold > 0)
    {
        WorldPacket* const packet = new WorldPacket(CMSG_LOOT_MONEY, 0);
        bot->GetSession()->QueuePacket(packet);
    }

    for (uint8 i = 0; i < items; ++i)
    {
        uint32 itemid;
        uint32 itemcount;
        uint8 lootslot_type;
        uint8 itemindex;
        bool grab = false;

        p >> itemindex;
        p >> itemid;
        p >> itemcount;
        p.read_skip<uint32>();  // display id
        p.read_skip<uint32>();  // randomSuffix
        p.read_skip<uint32>();  // randomPropertyId
        p >> lootslot_type;     // 0 = can get, 1 = look only, 2 = master get

        if (lootslot_type != LOOT_SLOT_NORMAL)
        {
            continue;
        }

        if (!IsLootAllowed(itemid))
        {
            continue;
        }

        if (sRandomPlayerbotMgr.IsRandomBot(bot))
        {
            ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(itemid);
            if (proto)
            {
                uint32 price = itemcount * auctionbot.GetSellPrice(proto) * sRandomPlayerbotMgr.GetSellMultiplier(bot) + gold;
                uint32 lootAmount = sRandomPlayerbotMgr.GetLootAmount(bot);
                if (bot->GetGroup() && price)
                {
                    sRandomPlayerbotMgr.SetLootAmount(bot, lootAmount + price);
                }
                else if (lootAmount)
                {
                    sRandomPlayerbotMgr.SetLootAmount(bot, 0);
                }
            }
        }

        WorldPacket* const packet = new WorldPacket(CMSG_AUTOSTORE_LOOT_ITEM, 1);
        *packet << itemindex;
        bot->GetSession()->QueuePacket(packet);
    }

    AI_VALUE(LootObjectStack*, "available loot")->Remove(guid);

    // release loot
    WorldPacket* const packet = new WorldPacket(CMSG_LOOT_RELEASE, 8);
    *packet << guid;
    bot->GetSession()->QueuePacket(packet);
    return true;
}

bool StoreLootAction::IsLootAllowed(uint32 itemid)
{
    LootStrategy lootStrategy = AI_VALUE(LootStrategy, "loot strategy");

    if (lootStrategy == LOOTSTRATEGY_ALL)
    {
        return true;
    }

    set<uint32>& lootItems = AI_VALUE(set<uint32>&, "always loot list");
    if (lootItems.find(itemid) != lootItems.end())
    {
        return true;
    }

    ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(itemid);
    if (!proto)
    {
        return false;
    }

    uint32 max = proto->MaxCount;
    if (max > 0 && bot->HasItemCount(itemid, max, true))
    {
        return false;
    }

    if (proto->StartQuest ||
        proto->Bonding == BIND_QUEST_ITEM ||
        proto->Bonding == BIND_QUEST_ITEM1 ||
        proto->Class == ITEM_CLASS_QUEST)
        return true;

    if (lootStrategy == LOOTSTRATEGY_QUEST)
    {
        return false;
    }

    ostringstream out; out << itemid;
    ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());
    if (usage == ITEM_USAGE_SKILL || usage == ITEM_USAGE_USE)
    {
        return true;
    }

    if (lootStrategy == LOOTSTRATEGY_SKILL)
    {
        return false;
    }

    if (proto->Quality == ITEM_QUALITY_POOR)
    {
        return true;
    }

    if (lootStrategy == LOOTSTRATEGY_GRAY)
    {
        return true;
    }

    if (proto->Bonding == BIND_WHEN_PICKED_UP)
    {
        return false;
    }

    return true;
}
