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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Resets the player's learned spells and restores default and quest rewards.
 */
void Player::resetSpells()
{
    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        RemoveAtLoginFlag(AT_LOGIN_RESET_SPELLS, true);
    }

    // make full copy of map (spells removed and marked as deleted at another spell remove
    // and we can't use original map for safe iterative with visit each spell at loop end
    PlayerSpellMap smap = GetSpellMap();

    for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
    {
        removeSpell(iter->first, false, false); // only iter->first can be accessed, object by iter->second can be deleted already
    }

    learnDefaultSpells();
    learnQuestRewardedSpells();
}

/**
 * @brief Teaches the player's default race and class spells.
 */
void Player::learnDefaultSpells()
{
    // learn default race/class spells
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    for (PlayerCreateInfoSpells::const_iterator itr = info->spell.begin(); itr != info->spell.end(); ++itr)
    {
        uint32 tspell = *itr;
        DEBUG_LOG("PLAYER (Class: %u Race: %u): Adding initial spell, id = %u", uint32(getClass()), uint32(getRace()), tspell);
        if (!IsInWorld())                                   // will send in INITIAL_SPELLS in list anyway at map add
        {
            addSpell(tspell, true, true, true, false);
        }
        else                                                // but send in normal spell in game learn case
        {
            learnSpell(tspell, true);
        }
    }
}

/**
 * @brief Teaches a quest reward spell if the quest grants a learnable spell.
 *
 * @param quest The rewarded quest to inspect.
 */
void Player::learnQuestRewardedSpells(Quest const* quest)
{
    uint32 spell_id = quest->GetRewSpellCast();

    // skip quests without rewarded spell
    if (!spell_id)
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        return;
    }

    // check learned spells state
    bool found = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_LEARN_SPELL && !HasSpell(spellInfo->EffectTriggerSpell[i]))
        {
            found = true;
            break;
        }
    }

    // skip quests with not teaching spell or already known spell
    if (!found)
    {
        return;
    }

    // prevent learn non first rank unknown profession and second specialization for same profession)
    uint32 learned_0 = spellInfo->EffectTriggerSpell[EFFECT_INDEX_0];
    if (sSpellMgr.GetSpellRank(learned_0) > 1 && !HasSpell(learned_0))
    {
        // not have first rank learned (unlearned prof?)
        uint32 first_spell = sSpellMgr.GetFirstSpellInChain(learned_0);
        if (!HasSpell(first_spell))
        {
            return;
        }

        SpellEntry const* learnedInfo = sSpellStore.LookupEntry(learned_0);
        if (!learnedInfo)
        {
            return;
        }

        // specialization
        if (learnedInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_TRADE_SKILL && learnedInfo->Effect[EFFECT_INDEX_1] == 0)
        {
            // search other specialization for same prof
            for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
            {
                if (itr->second.state == PLAYERSPELL_REMOVED || itr->first == learned_0)
                {
                    continue;
                }

                SpellEntry const* itrInfo = sSpellStore.LookupEntry(itr->first);
                if (!itrInfo)
                {
                    return;
                }

                // compare only specializations
                if (itrInfo->Effect[EFFECT_INDEX_0] != SPELL_EFFECT_TRADE_SKILL || itrInfo->Effect[EFFECT_INDEX_1] != 0)
                {
                    continue;
                }

                // compare same chain spells
                if (sSpellMgr.GetFirstSpellInChain(itr->first) != first_spell)
                {
                    continue;
                }

                // now we have 2 specialization, learn possible only if found is lesser specialization rank
                if (!sSpellMgr.IsHighRankOfSpell(learned_0, itr->first))
                {
                    return;
                }
            }
        }
    }

    CastSpell(this, spell_id, true);
}

/**
 * @brief Reapplies all quest reward spells from previously rewarded quests.
 */
void Player::learnQuestRewardedSpells()
{
    // learn spells received from quest completing
    for (QuestStatusMap::const_iterator itr = mQuestStatus.begin(); itr != mQuestStatus.end(); ++itr)
    {
        // skip no rewarded quests
        if (!itr->second.m_rewarded)
        {
            continue;
        }

        Quest const* quest = sObjectMgr.GetQuestTemplate(itr->first);
        if (!quest)
        {
            continue;
        }

        learnQuestRewardedSpells(quest);
    }
}

/**
 * @brief Learns or removes spells unlocked by a profession or skill value.
 *
 * @param skill_id The skill line identifier.
 * @param skill_value The current skill value.
 */
void Player::learnSkillRewardedSpells(uint32 skill_id, uint32 skill_value)
{
    uint32 raceMask  = getRaceMask();
    uint32 classMask = getClassMask();
    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j);
        if (!pAbility || pAbility->skillId != skill_id || pAbility->learnOnGetSkill != ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
        {
            continue;
        }
        // Check race if set
        if (pAbility->racemask && !(pAbility->racemask & raceMask))
        {
            continue;
        }
        // Check class if set
        if (pAbility->classmask && !(pAbility->classmask & classMask))
        {
            continue;
        }

        if (sSpellStore.LookupEntry(pAbility->spellId))
        {
            // need unlearn spell
            if (skill_value < pAbility->req_skill_value)
            {
                removeSpell(pAbility->spellId);
            }
            // need learn
            else if (!IsInWorld())
            {
                addSpell(pAbility->spellId, true, true, true, false);
            }
            else
            {
                learnSpell(pAbility->spellId, true);
            }
        }
    }
}
