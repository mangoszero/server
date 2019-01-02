/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Common.h"
#include "CreatureEventAI.h"
#include "CreatureEventAIMgr.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceData.h"
#include "Chat.h"
#include "Language.h"

bool CreatureEventAIHolder::UpdateRepeatTimer(Creature* creature, uint32 repeatMin, uint32 repeatMax)
{
    if (repeatMin == repeatMax)
        { Time = repeatMin; }
    else if (repeatMax > repeatMin)
        { Time = urand(repeatMin, repeatMax); }
    else
    {
        sLog.outErrorEventAI("Creature %u using Event %u (Type = %u) has RandomMax < RandomMin. Event repeating disabled.", creature->GetEntry(), Event.event_id, Event.event_type);
        Enabled = false;
        return false;
    }

    return true;
}

int CreatureEventAI::Permissible(const Creature* creature)
{
    if (creature->GetAIName() == "EventAI")
        { return PERMIT_BASE_SPECIAL; }
    return PERMIT_BASE_NO;
}

void CreatureEventAI::GetAIInformation(ChatHandler& reader)
{
    reader.PSendSysMessage(LANG_NPC_EVENTAI_PHASE, (uint32)m_Phase);
    reader.PSendSysMessage(LANG_NPC_EVENTAI_MOVE, reader.GetOnOffStr(IsCombatMovement()));
    reader.PSendSysMessage(LANG_NPC_EVENTAI_COMBAT, reader.GetOnOffStr(m_MeleeEnabled));

    if (sLog.HasLogFilter(LOG_FILTER_EVENT_AI_DEV))         // Give some more details if in EventAI Dev Mode
        { return; }

    reader.PSendSysMessage("Current events of this creature:");
    for (CreatureEventAIList::const_iterator itr = m_CreatureEventAIList.begin(); itr != m_CreatureEventAIList.end(); ++itr)
    {
        if (itr->Event.action[2].type != ACTION_T_NONE)
            { reader.PSendSysMessage("%u Type%3u (%s) Timer(%3us) actions[type(param1)]: %2u(%5u)  --  %2u(%u)  --  %2u(%5u)", itr->Event.event_id, itr->Event.event_type, itr->Enabled ? "On" : "Off", itr->Time / 1000, itr->Event.action[0].type, itr->Event.action[0].raw.param1, itr->Event.action[1].type, itr->Event.action[1].raw.param1, itr->Event.action[2].type, itr->Event.action[2].raw.param1); }
        else if (itr->Event.action[1].type != ACTION_T_NONE)
            { reader.PSendSysMessage("%u Type%3u (%s) Timer(%3us) actions[type(param1)]: %2u(%5u)  --  %2u(%5u)", itr->Event.event_id, itr->Event.event_type, itr->Enabled ? "On" : "Off", itr->Time / 1000, itr->Event.action[0].type, itr->Event.action[0].raw.param1, itr->Event.action[1].type, itr->Event.action[1].raw.param1); }
        else
            { reader.PSendSysMessage("%u Type%3u (%s) Timer(%3us) action[type(param1)]:  %2u(%5u)", itr->Event.event_id, itr->Event.event_type, itr->Enabled ? "On" : "Off", itr->Time / 1000, itr->Event.action[0].type, itr->Event.action[0].raw.param1); }
    }
}

CreatureEventAI::CreatureEventAI(Creature* c) : CreatureAI(c),
    m_Phase(0),
    m_MeleeEnabled(true),
    m_currSpell(0),
    m_HasOOCLoSEvent(false),
    m_InvinceabilityHpLevel(0),
    m_throwAIEventMask(0),
    m_throwAIEventStep(0)
{
    // Need make copy for filter unneeded steps and safe in case table reload
    CreatureEventAI_Event_Map::const_iterator creatureEventsItr = sEventAIMgr.GetCreatureEventAIMap().find(m_creature->GetEntry());
    if (creatureEventsItr != sEventAIMgr.GetCreatureEventAIMap().end())
    {
        uint32 events_count = 0;

        const CreatureEventAI_Event_Vec &creatureEvent = creatureEventsItr->second;
        for (CreatureEventAI_Event_Vec::const_iterator i = creatureEvent.begin(); i != creatureEvent.end(); ++i)
        {
            // Debug check
#ifndef MANGOS_DEBUG
            if (i->event_flags & EFLAG_DEBUG_ONLY)
                continue;
#endif

            ++events_count;
        }
        // EventMap had events but they were not added because they must be for instance
        if (events_count == 0)
            { sLog.outErrorEventAI("Creature %u has events but no events added to list because of instance flags (spawned in map %u).", m_creature->GetEntry(), m_creature->GetMapId()); }
        else
        {
            m_CreatureEventAIList.reserve(events_count);
            for (CreatureEventAI_Event_Vec::const_iterator i = (*creatureEventsItr).second.begin(); i != (*creatureEventsItr).second.end(); ++i)
            {
                // Debug check
#ifndef MANGOS_DEBUG
                if (i->event_flags & EFLAG_DEBUG_ONLY)
                    { continue; }
#endif
                m_CreatureEventAIList.push_back(CreatureEventAIHolder(*i));
                // Cache for fast use
                if (i->event_type == EVENT_T_OOC_LOS)
                    { m_HasOOCLoSEvent = true; }
            }
        }
    }
    else
        { sLog.outErrorEventAI("EventMap for Creature %u is empty but creature is using CreatureEventAI.", m_creature->GetEntry()); }

    // Handle Spawned Events, also calls Reset()
    JustRespawned();
}

#define LOG_PROCESS_EVENT                                                                                                       \
    DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: Event type %u (script %u) triggered for %s (invoked by %s)",    \
                     pHolder.Event.event_type, pHolder.Event.event_id, m_creature->GetGuidStr().c_str(), pActionInvoker ? pActionInvoker->GetGuidStr().c_str() : "<no invoker>")

inline bool IsTimerBasedEvent(EventAI_Type type)
{
    switch (type)
    {
        case EVENT_T_TIMER_IN_COMBAT:
        case EVENT_T_TIMER_OOC:
        case EVENT_T_TIMER_GENERIC:
        case EVENT_T_MANA:
        case EVENT_T_HP:
        case EVENT_T_TARGET_HP:
        case EVENT_T_TARGET_CASTING:
        case EVENT_T_FRIENDLY_HP:
        case EVENT_T_FRIENDLY_IS_CC:
        case EVENT_T_AURA:
        case EVENT_T_TARGET_AURA:
        case EVENT_T_MISSING_AURA:
        case EVENT_T_TARGET_MISSING_AURA:
        case EVENT_T_RANGE:
        case EVENT_T_ENERGY:
            return true;
        default:
            return false;
    }
}

bool CreatureEventAI::ProcessEvent(CreatureEventAIHolder& pHolder, Unit* pActionInvoker, Creature* pAIEventSender /*=NULL*/)
{
    if (!pHolder.Enabled || pHolder.Time)
        { return false; }

    // Check the inverse phase mask (event doesn't trigger if current phase bit is set in mask)
    if (pHolder.Event.event_inverse_phase_mask & (1 << m_Phase))
    {
        if (!IsTimerBasedEvent(pHolder.Event.event_type))
            { DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: Event %u skipped because of phasemask %u. Current phase %u", pHolder.Event.event_id, pHolder.Event.event_inverse_phase_mask, m_Phase); }
        return false;
    }

    if (!IsTimerBasedEvent(pHolder.Event.event_type))
        { LOG_PROCESS_EVENT; }

    CreatureEventAI_Event const& event = pHolder.Event;

    // Check event conditions based on the event type, also reset events
    switch (event.event_type)
    {
        case EVENT_T_TIMER_IN_COMBAT:
            if (!m_creature->IsInCombat())
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.timer.repeatMin, event.timer.repeatMax);
            break;
        case EVENT_T_TIMER_OOC:
            if (m_creature->IsInCombat() || m_creature->IsInEvadeMode())
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.timer.repeatMin, event.timer.repeatMax);
            break;
        case EVENT_T_TIMER_GENERIC:
            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.timer.repeatMin, event.timer.repeatMax);
            break;
        case EVENT_T_HP:
        {
            if (!m_creature->IsInCombat() || !m_creature->GetMaxHealth())
                { return false; }

            uint32 perc = (m_creature->GetHealth() * 100) / m_creature->GetMaxHealth();

            if (perc > event.percent_range.percentMax || perc < event.percent_range.percentMin)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.percent_range.repeatMin, event.percent_range.repeatMax);
            break;
        }
        case EVENT_T_MANA:
        {
            if (!m_creature->IsInCombat() || !m_creature->GetMaxPower(POWER_MANA))
                { return false; }

            uint32 perc = (m_creature->GetPower(POWER_MANA) * 100) / m_creature->GetMaxPower(POWER_MANA);

            if (perc > event.percent_range.percentMax || perc < event.percent_range.percentMin)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.percent_range.repeatMin, event.percent_range.repeatMax);
            break;
        }
        case EVENT_T_AGGRO:
            break;
        case EVENT_T_KILL:
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.kill.repeatMin, event.kill.repeatMax);
            break;
        case EVENT_T_DEATH:
        case EVENT_T_EVADE:
            break;
        case EVENT_T_SPELLHIT:
            // Spell hit is special case, param1 and param2 handled within CreatureEventAI::SpellHit

            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.spell_hit.repeatMin, event.spell_hit.repeatMax);
            break;
        case EVENT_T_RANGE:
            if (!m_creature->IsInCombat() || !m_creature->getVictim() || !m_creature->IsInMap(m_creature->getVictim()))
                return false;

            // DISCUSS TODO - Likely replace IsInRange check with CombatReach checks (as used rather for such checks)
            if (!m_creature->IsInRange(m_creature->getVictim(), (float)event.range.minDist, (float)event.range.maxDist))
                return false;

            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.range.repeatMin, event.range.repeatMax);
            break;
        case EVENT_T_OOC_LOS:
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.ooc_los.repeatMin, event.ooc_los.repeatMax);
            break;
        case EVENT_T_SPAWNED:
            break;
        case EVENT_T_TARGET_HP:
        {
            if (!m_creature->IsInCombat() || !m_creature->getVictim() || !m_creature->getVictim()->GetMaxHealth())
                { return false; }

            uint32 perc = (m_creature->getVictim()->GetHealth() * 100) / m_creature->getVictim()->GetMaxHealth();

            if (perc > event.percent_range.percentMax || perc < event.percent_range.percentMin)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.percent_range.repeatMin, event.percent_range.repeatMax);
            break;
        }
        case EVENT_T_TARGET_CASTING:
            if (!m_creature->IsInCombat() || !m_creature->getVictim() || !m_creature->getVictim()->IsNonMeleeSpellCasted(false, false, true))
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.target_casting.repeatMin, event.target_casting.repeatMax);
            break;
        case EVENT_T_FRIENDLY_HP:
        {
            if (!m_creature->IsInCombat())
                { return false; }

            Unit* pUnit = DoSelectLowestHpFriendly((float)event.friendly_hp.radius, event.friendly_hp.hpDeficit);
            if (!pUnit)
                { return false; }

            pActionInvoker = pUnit;

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.friendly_hp.repeatMin, event.friendly_hp.repeatMax);
            break;
        }
        case EVENT_T_FRIENDLY_IS_CC:
        {
            if (!m_creature->IsInCombat())
                { return false; }

            std::list<Creature*> pList;
            DoFindFriendlyCC(pList, (float)event.friendly_is_cc.radius);

            // List is empty
            if (pList.empty())
                { return false; }

            // We don't really care about the whole list, just return first available
            pActionInvoker = *(pList.begin());

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.friendly_is_cc.repeatMin, event.friendly_is_cc.repeatMax);
            break;
        }
        case EVENT_T_FRIENDLY_MISSING_BUFF:
        {
            std::list<Creature*> pList;
            DoFindFriendlyMissingBuff(pList, (float)event.friendly_buff.radius, event.friendly_buff.spellId);

            // List is empty
            if (pList.empty())
                { return false; }

            // We don't really care about the whole list, just return first available
            pActionInvoker = *(pList.begin());

            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.friendly_buff.repeatMin, event.friendly_buff.repeatMax);
            break;
        }
        case EVENT_T_SUMMONED_UNIT:
        case EVENT_T_SUMMONED_JUST_DIED:
        case EVENT_T_SUMMONED_JUST_DESPAWN:
        {
            // Prevent event from occuring on no unit or non creatures
            if (!pActionInvoker || pActionInvoker->GetTypeId() != TYPEID_UNIT)
                { return false; }

            // Creature id doesn't match up
            if (((Creature*)pActionInvoker)->GetEntry() != event.summoned.creatureId)
                { return false; }

            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.summoned.repeatMin, event.summoned.repeatMax);
            break;
        }
        case EVENT_T_TARGET_MANA:
        {
            if (!m_creature->IsInCombat() || !m_creature->getVictim() || !m_creature->getVictim()->GetMaxPower(POWER_MANA))
                { return false; }

            uint32 perc = (m_creature->getVictim()->GetPower(POWER_MANA) * 100) / m_creature->getVictim()->GetMaxPower(POWER_MANA);

            if (perc > event.percent_range.percentMax || perc < event.percent_range.percentMin)
                { return false; }

            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.percent_range.repeatMin, event.percent_range.repeatMax);
            break;
        }
        case EVENT_T_REACHED_HOME:
        case EVENT_T_RECEIVE_EMOTE:
            break;
        case EVENT_T_AURA:
        {
            if (!m_creature->IsInCombat())
                { return false; }

            SpellAuraHolder* holder = m_creature->GetSpellAuraHolder(event.buffed.spellId);
            if (!holder || holder->GetStackAmount() < event.buffed.amount)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.buffed.repeatMin, event.buffed.repeatMax);
            break;
        }
        case EVENT_T_TARGET_AURA:
        {
            if (!m_creature->IsInCombat() || !m_creature->getVictim())
                { return false; }

            SpellAuraHolder* holder = m_creature->getVictim()->GetSpellAuraHolder(event.buffed.spellId);
            if (!holder || holder->GetStackAmount() < event.buffed.amount)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.buffed.repeatMin, event.buffed.repeatMax);
            break;
        }
        case EVENT_T_MISSING_AURA:
        {
            if (!m_creature->IsInCombat())
                { return false; }

            SpellAuraHolder* holder = m_creature->GetSpellAuraHolder(event.buffed.spellId);
            if (holder && holder->GetStackAmount() >= event.buffed.amount)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.buffed.repeatMin, event.buffed.repeatMax);
            break;
        }
        case EVENT_T_TARGET_MISSING_AURA:
        {
            if (!m_creature->IsInCombat() || !m_creature->getVictim())
                { return false; }

            SpellAuraHolder* holder = m_creature->getVictim()->GetSpellAuraHolder(event.buffed.spellId);
            if (holder && holder->GetStackAmount() >= event.buffed.amount)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.buffed.repeatMin, event.buffed.repeatMax);
            break;
        }
        case EVENT_T_RECEIVE_AI_EVENT:
            break;
        case EVENT_T_REACHED_WAYPOINT:
            if (!m_creature->IsNearWaypoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), event.reached_waypoint.positionX, event.reached_waypoint.positionY, event.reached_waypoint.positionZ, event.reached_waypoint.distance, event.reached_waypoint.distance, event.reached_waypoint.distance))
            {
                return false;
            }

            LOG_PROCESS_EVENT;
            break;
        case EVENT_T_ENERGY:
        {
            if (!m_creature->IsInCombat() || !m_creature->GetMaxPower(POWER_ENERGY))
                { return false; }

            uint32 perc = (m_creature->GetPower(POWER_ENERGY) * 100) / m_creature->GetMaxPower(POWER_ENERGY);

            if (perc > event.percent_range.percentMax || perc < event.percent_range.percentMin)
                { return false; }

            LOG_PROCESS_EVENT;
            // Repeat Timers
            pHolder.UpdateRepeatTimer(m_creature, event.percent_range.repeatMin, event.percent_range.repeatMax);
            break;
        }
        default:
            sLog.outErrorEventAI("Creature %u using Event %u has invalid Event Type(%u), missing from ProcessEvent() Switch.", m_creature->GetEntry(), pHolder.Event.event_id, pHolder.Event.event_type);
            break;
    }

    // Disable non-repeatable events
    if (!(pHolder.Event.event_flags & EFLAG_REPEATABLE))
        { pHolder.Enabled = false; }

    // Store random here so that all random actions match up
    uint32 rnd = rand();

    // Return if chance for event is not met
    if (pHolder.Event.event_chance <= rnd % 100)
        { return false; }

    // Process actions, normal case
    if (!(pHolder.Event.event_flags & EFLAG_RANDOM_ACTION))
    {
        for (uint32 j = 0; j < MAX_ACTIONS; ++j)
            { ProcessAction(pHolder.Event.action[j], rnd, pHolder.Event.event_id, pActionInvoker, pAIEventSender); }
    }
    // Process actions, random case
    else
    {
        // amount of real actions
        uint32 count = 0;
        for (uint32 j = 0; j < MAX_ACTIONS; ++j)
            if (pHolder.Event.action[j].type != ACTION_T_NONE)
                { ++count; }

        if (count)
        {
            // select action number from found amount
            uint32 idx = rnd % count;

            // find selected action, skipping not used
            uint32 j = 0;
            for (; ; ++j)
            {
                if (pHolder.Event.action[j].type != ACTION_T_NONE)
                {
                    if (!idx)
                        { break; }
                    --idx;
                }
            }

            ProcessAction(pHolder.Event.action[j], rnd, pHolder.Event.event_id, pActionInvoker, pAIEventSender);
        }
    }
    return true;
}

void CreatureEventAI::ProcessAction(CreatureEventAI_Action const& action, uint32 rnd, uint32 EventId, Unit* pActionInvoker, Creature* pAIEventSender)
{
    if (action.type == ACTION_T_NONE)          //0
        { return; }

    DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: Process action %u (script %u) triggered for %s (invoked by %s)",
                     action.type, EventId, m_creature->GetGuidStr().c_str(), pActionInvoker ? pActionInvoker->GetGuidStr().c_str() : "<no invoker>");

    bool reportTargetError = false;
    switch (action.type)
    {
        case ACTION_T_TEXT:                   //1
        case ACTION_T_CHANCED_TEXT:           //44
        {
            if (!action.text.TextId[0])
                { return; }

            int32 textId = 0;

            if (action.type == ACTION_T_TEXT)
            {
                if (action.text.TextId[1] && action.text.TextId[2])
                    { textId = action.text.TextId[rnd % 3]; }
                else if (action.text.TextId[1] && (rnd % 2))
                    { textId = action.text.TextId[1]; }
                else
                    { textId = action.text.TextId[0]; }
            }
            // ACTION_T_CHANCED_TEXT, chance hits
            else if ((rnd % 100) < action.chanced_text.chance)
            {
                if (action.chanced_text.TextId[0] && action.chanced_text.TextId[1])
                    { textId = action.chanced_text.TextId[rnd % 2]; }
                else
                    { textId = action.chanced_text.TextId[0]; }
            }

            if (textId)
            {
                Unit* target = NULL;

                if (pActionInvoker)
                {
                    if (pActionInvoker->GetTypeId() == TYPEID_PLAYER)
                        { target = pActionInvoker; }
                    else if (Unit* owner = pActionInvoker->GetOwner())
                    {
                        if (owner->GetTypeId() == TYPEID_PLAYER)
                            { target = owner; }
                    }
                }
                else if ((target = m_creature->getVictim()))
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                        if (Unit* owner = target->GetOwner())
                            if (owner->GetTypeId() == TYPEID_PLAYER)
                                { target = owner; }
                }

                if (!DoDisplayText(m_creature, textId, target))
                    { sLog.outErrorEventAI("Error attempting to display text %i, used by script %u", textId, EventId); }
            }
            break;
        }
        case ACTION_T_SET_FACTION:                  //2
        {
            if (action.set_faction.factionId)
                { m_creature->SetFactionTemporary(action.set_faction.factionId, action.set_faction.factionFlags); }
            else                                            // no id provided, assume reset and then use default
                { m_creature->ClearTemporaryFaction(); }

            break;
        }
        case ACTION_T_MORPH_TO_ENTRY_OR_MODEL:      //3
        {
            if (action.morph.creatureId || action.morph.modelId)
            {
                // set model based on entry from creature_template
                if (action.morph.creatureId)
                {
                    if (CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(action.morph.creatureId))
                    {
                        uint32 display_id = Creature::ChooseDisplayId(ci);
                        m_creature->SetDisplayId(display_id);
                    }
                }
                // if no param1, then use value from param2 (modelId)
                else
                    { m_creature->SetDisplayId(action.morph.modelId); }
            }
            else
                { m_creature->DeMorph(); }
            break;
        }
        case ACTION_T_SOUND:                    //4
            m_creature->PlayDirectSound(action.sound.soundId);
            break;
        case ACTION_T_EMOTE:                    //5
            m_creature->HandleEmote(action.emote.emoteId);
            break;
        case ACTION_T_RANDOM_SOUND:             //9
        {
            int32 temp = GetRandActionParam(rnd, action.random_sound.soundId1, action.random_sound.soundId2, action.random_sound.soundId3);
            if (temp >= 0)
                { m_creature->PlayDirectSound(temp); }
            break;
        }
        case ACTION_T_RANDOM_EMOTE:             //10
        {
            int32 temp = GetRandActionParam(rnd, action.random_emote.emoteId1, action.random_emote.emoteId2, action.random_emote.emoteId3);
            if (temp >= 0)
                { m_creature->HandleEmote(temp); }
            break;
        }
        case ACTION_T_CAST:                     //11
        {
            uint32 selectFlags = 0;
            uint32 spellId = 0;

            if (!(action.cast.castFlags & (CAST_TRIGGERED | CAST_FORCE_CAST | CAST_FORCE_TARGET_SELF)))
            {
                spellId = action.cast.spellId;
                selectFlags = SELECT_FLAG_IN_LOS;
            }

            Unit* target = GetTargetByType(action.cast.target, pActionInvoker, pAIEventSender, reportTargetError, spellId, selectFlags);
            if (!target)
            {
                if (reportTargetError)
                    { sLog.outErrorEventAI("NULL target for ACTION_T_CAST %s casting spell id %u", m_creature->GetGuidStr().c_str(), action.cast.spellId); }
                return;
            }

            DoCastSpellIfCan(target, action.cast.spellId, action.cast.castFlags);

        break;
        }
        case ACTION_T_SUMMON:               //12
        {
            Unit* target = GetTargetByType(action.summon.target, pActionInvoker, pAIEventSender, reportTargetError);
            if (!target && reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_SUMMON(%u), target-type %u", EventId, action.type, action.summon.target); }

            Creature* pCreature;

            if (action.summon.duration)
                { pCreature = m_creature->SummonCreature(action.summon.creatureId, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, action.summon.duration); }
            else
                { pCreature = m_creature->SummonCreature(action.summon.creatureId, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_OOC_DESPAWN, 0); }

            if (!pCreature)
                { sLog.outErrorEventAI("failed to spawn creature %u. Spawn event %d is on creature %d", action.summon.creatureId, EventId, m_creature->GetEntry()); }
            else if (action.summon.target != TARGET_T_SELF && target)
                { pCreature->AI()->AttackStart(target); }
            break;
        }
        case ACTION_T_THREAT_SINGLE_PCT:    //13
            if (Unit* target = GetTargetByType(action.threat_single_pct.target, pActionInvoker, pAIEventSender, reportTargetError))
                { m_creature->GetThreatManager().modifyThreatPercent(target, action.threat_single_pct.percent); }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_THREAT_SINGLE_PCT(%u), target-type %u", EventId, action.type, action.threat_single_pct.target); }
            break;
        case ACTION_T_THREAT_ALL_PCT:       //14
        {
            ThreatList const& threatList = m_creature->GetThreatManager().getThreatList();
            for (ThreatList::const_iterator i = threatList.begin(); i != threatList.end(); ++i)
                if (Unit* Temp = m_creature->GetMap()->GetUnit((*i)->getUnitGuid()))
                    { m_creature->GetThreatManager().modifyThreatPercent(Temp, action.threat_all_pct.percent); }
            break;
        }
        case ACTION_T_QUEST_EVENT:          //15
            if (Unit* target = GetTargetByType(action.quest_event.target, pActionInvoker, pAIEventSender, reportTargetError))
            {
                if (target->GetTypeId() == TYPEID_PLAYER)
                    { ((Player*)target)->AreaExploredOrEventHappens(action.quest_event.questId); }
            }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_QUEST_EVENT(%u), target-type %u", EventId, action.type, action.quest_event.target); }
            break;
        case ACTION_T_CAST_EVENT:           //16
            if (Unit* target = GetTargetByType(action.cast_event.target, pActionInvoker, pAIEventSender, reportTargetError, 0, SELECT_FLAG_PLAYER))
            {
                if (target->GetTypeId() == TYPEID_PLAYER)
                    { ((Player*)target)->CastedCreatureOrGO(action.cast_event.creatureId, m_creature->GetObjectGuid(), action.cast_event.spellId); }
            }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_CST_EVENT(%u), target-type %u", EventId, action.type, action.cast_event.target); }
            break;
        case ACTION_T_SET_UNIT_FIELD:       //17
        {
            Unit* target = GetTargetByType(action.set_unit_field.target, pActionInvoker, pAIEventSender, reportTargetError);

            // not allow modify important for integrity object fields
            if (action.set_unit_field.field < OBJECT_END || action.set_unit_field.field >= UNIT_END)
                { return; }

            if (target)
                { target->SetUInt32Value(action.set_unit_field.field, action.set_unit_field.value); }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_SET_UNIT_FIELD(%u), target-type %u", EventId, action.type, action.set_unit_field.target); }

            break;
        }
        case ACTION_T_SET_UNIT_FLAG:        //18
            if (Unit* target = GetTargetByType(action.unit_flag.target, pActionInvoker, pAIEventSender, reportTargetError))
                { target->SetFlag(UNIT_FIELD_FLAGS, action.unit_flag.value); }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_SET_UNIT_FLAG(%u), target-type %u", EventId, action.type, action.unit_flag.target); }
            break;
        case ACTION_T_REMOVE_UNIT_FLAG:     //19
            if (Unit* target = GetTargetByType(action.unit_flag.target, pActionInvoker, pAIEventSender, reportTargetError))
                { target->RemoveFlag(UNIT_FIELD_FLAGS, action.unit_flag.value); }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_REMOVE_UNIT_FLAG(%u), target-type %u", EventId, action.type, action.unit_flag.target); }
            break;
        case ACTION_T_AUTO_ATTACK:          //20
            m_MeleeEnabled = action.auto_attack.state != 0;
            break;
        case ACTION_T_COMBAT_MOVEMENT:      //21
        {
            // ignore no affect case
            if ((GetCombatMovementFlags() & CM_SCRIPT) == (action.combat_movement.state != 0))
                return;

            SetCombatMovement(action.combat_movement.state != 0, true);

            if (m_creature->IsInCombat() && m_creature->getVictim())
            {
                if (IsCombatMovement() && action.combat_movement.melee)
                    m_creature->SendMeleeAttackStart(m_creature->getVictim());
                else if (action.combat_movement.melee)
                    m_creature->SendMeleeAttackStop(m_creature->getVictim());
            }
            break;
        }
        case ACTION_T_SET_PHASE:            //22
            m_Phase = action.set_phase.phase;
            DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: ACTION_T_SET_PHASE - script %u for %s, phase is now %u", EventId, m_creature->GetGuidStr().c_str(), m_Phase);
            break;
        case ACTION_T_INC_PHASE:            //23
        {
            int32 new_phase = int32(m_Phase) + action.set_inc_phase.step;
            if (new_phase < 0)
            {
                sLog.outErrorEventAI("Event %d decrease Phase under 0. CreatureEntry = %d", EventId, m_creature->GetEntry());
                m_Phase = 0;
            }
            else if (new_phase >= MAX_PHASE)
            {
                sLog.outErrorEventAI("Event %d incremented Phase above %u. Phase mask can not be used with phases past %u. CreatureEntry = %d", EventId, MAX_PHASE - 1, MAX_PHASE - 1, m_creature->GetEntry());
                m_Phase = MAX_PHASE - 1;
            }
            else
                { m_Phase = new_phase; }

            DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: ACTION_T_INC_PHASE - script %u for %s, phase is now %u", EventId, m_creature->GetGuidStr().c_str(), m_Phase);
            break;
        }
        case ACTION_T_EVADE:                 //24
            EnterEvadeMode();
            break;
        case ACTION_T_FLEE_FOR_ASSIST:      //25
            m_creature->DoFleeToGetAssistance();
            break;
        case ACTION_T_QUEST_EVENT_ALL:      //26
            if (pActionInvoker && pActionInvoker->GetTypeId() == TYPEID_PLAYER)
                { ((Player*)pActionInvoker)->GroupEventHappens(action.quest_event_all.questId, m_creature); }
            break;
        case ACTION_T_CAST_EVENT_ALL:       //27
        {
            ThreatList const& threatList = m_creature->GetThreatManager().getThreatList();
            for (ThreatList::const_iterator i = threatList.begin(); i != threatList.end(); ++i)
                if (Player* temp = m_creature->GetMap()->GetPlayer((*i)->getUnitGuid()))
                    { temp->CastedCreatureOrGO(action.cast_event_all.creatureId, m_creature->GetObjectGuid(), action.cast_event_all.spellId); }
            break;
        }
        case ACTION_T_REMOVEAURASFROMSPELL:     //28
            if (Unit* target = GetTargetByType(action.remove_aura.target, pActionInvoker, pAIEventSender, reportTargetError))
                { target->RemoveAurasDueToSpell(action.remove_aura.spellId); }
            else if (reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_REMOVEAURASFROMSPELL(%u), target-type %u", EventId, action.type, action.remove_aura.target); }
            break;
        case ACTION_T_RANGED_MOVEMENT:          //29
            m_attackDistance = (float)action.ranged_movement.distance;
            m_attackAngle = action.ranged_movement.angle / 180.0f * M_PI_F;

            if (IsCombatMovement())
            {
                if (m_creature->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
                {
                    // Drop current movement gen
                    m_creature->GetMotionMaster()->Clear(false);
                    m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim(), m_attackDistance, m_attackAngle);
                }
            }
            break;
        case ACTION_T_RANDOM_PHASE:             //30
            m_Phase = GetRandActionParam(rnd, action.random_phase.phase1, action.random_phase.phase2, action.random_phase.phase3);
            DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: ACTION_T_RANDOM_PHASE - script %u for %s, phase is now %u", EventId, m_creature->GetGuidStr().c_str(), m_Phase);
            break;
        case ACTION_T_RANDOM_PHASE_RANGE:       //31
            if (action.random_phase_range.phaseMax > action.random_phase_range.phaseMin)
                { m_Phase = action.random_phase_range.phaseMin + (rnd % (action.random_phase_range.phaseMax - action.random_phase_range.phaseMin)); }
            else
                { sLog.outErrorEventAI("ACTION_T_RANDOM_PHASE_RANGE can not have Param2 <= Param1. Divide by Zero. Event = %d. CreatureEntry = %d", EventId, m_creature->GetEntry()); }
            break;
        case ACTION_T_SUMMON_ID:                //32
        {
            Unit* target = GetTargetByType(action.summon_id.target, pActionInvoker, pAIEventSender, reportTargetError);
            if (!target && reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_SUMMON_ID(%u), target-type %u", EventId, action.type, action.summon_id.target); }

            CreatureEventAI_Summon_Map::const_iterator i = sEventAIMgr.GetCreatureEventAISummonMap().find(action.summon_id.spawnId);
            if (i == sEventAIMgr.GetCreatureEventAISummonMap().end())
            {
                sLog.outErrorEventAI("failed to spawn creature %u. Summon map index %u does not exist. EventID %d. CreatureID %d", action.summon_id.creatureId, action.summon_id.spawnId, EventId, m_creature->GetEntry());
                return;
            }

            Creature* pCreature;
            if (i->second.SpawnTimeSecs)
                { pCreature = m_creature->SummonCreature(action.summon_id.creatureId, i->second.position_x, i->second.position_y, i->second.position_z, i->second.orientation, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, i->second.SpawnTimeSecs); }
            else
                { pCreature = m_creature->SummonCreature(action.summon_id.creatureId, i->second.position_x, i->second.position_y, i->second.position_z, i->second.orientation, TEMPSUMMON_TIMED_OOC_DESPAWN, 0); }

            if (!pCreature)
                { sLog.outErrorEventAI("failed to spawn creature %u. EventId %d.Creature %d", action.summon_id.creatureId, EventId, m_creature->GetEntry()); }
            else if (action.summon_id.target != TARGET_T_SELF && target)
                { pCreature->AI()->AttackStart(target); }

            break;
        }
        case ACTION_T_KILLED_MONSTER:       //33
            // first attempt player/group who tapped creature
            if (Player* pPlayer = m_creature->GetLootRecipient())
                { pPlayer->RewardPlayerAndGroupAtEvent(action.killed_monster.creatureId, m_creature); }
            else
            {
                // if not available, use pActionInvoker
                if (Unit* pTarget = GetTargetByType(action.killed_monster.target, pActionInvoker, pAIEventSender, reportTargetError, 0, SELECT_FLAG_PLAYER))
                {
                    if (Player* pPlayer2 = pTarget->GetCharmerOrOwnerPlayerOrPlayerItself())
                        { pPlayer2->RewardPlayerAndGroupAtEvent(action.killed_monster.creatureId, m_creature); }
                }
                else if (reportTargetError)
                    { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_KILLED_MONSTER(%u), target-type %u", EventId, action.type, action.killed_monster.target); }
            }
            break;
        case ACTION_T_SET_INST_DATA:        //34
        {
            InstanceData* pInst = m_creature->GetInstanceData();
            if (!pInst)
            {
                sLog.outErrorEventAI("Event %d attempt to set instance data without instance script. Creature %d", EventId, m_creature->GetEntry());
                return;
            }

            pInst->SetData(action.set_inst_data.field, action.set_inst_data.value);
            break;
        }
        case ACTION_T_SET_INST_DATA64:      //35
        {
            Unit* target = GetTargetByType(action.set_inst_data64.target, pActionInvoker, pAIEventSender, reportTargetError);
            if (!target)
            {
                if (reportTargetError)
                    { sLog.outErrorEventAI("Event %d attempt to set instance data64 but Target == NULL. Creature %d", EventId, m_creature->GetEntry()); }
                return;
            }

            InstanceData* pInst = m_creature->GetInstanceData();
            if (!pInst)
            {
                sLog.outErrorEventAI("Event %d attempt to set instance data64 without instance script. Creature %d", EventId, m_creature->GetEntry());
                return;
            }

            pInst->SetData64(action.set_inst_data64.field, target->GetObjectGuid().GetRawValue());
            break;
        }
        case ACTION_T_UPDATE_TEMPLATE:      //36
            if (m_creature->GetEntry() == action.update_template.creatureId)
            {
                sLog.outErrorEventAI("Event %d ACTION_T_UPDATE_TEMPLATE call with param1 == current entry. Creature %d", EventId, m_creature->GetEntry());
                return;
            }

            m_creature->UpdateEntry(action.update_template.creatureId, action.update_template.team ? HORDE : ALLIANCE);
            break;
        case ACTION_T_DIE:                  //37
            if (m_creature->IsDead())
            {
                sLog.outErrorEventAI("Event %d ACTION_T_DIE on dead creature. Creature %d", EventId, m_creature->GetEntry());
                return;
            }
            m_creature->DealDamage(m_creature, m_creature->GetMaxHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            break;
        case ACTION_T_ZONE_COMBAT_PULSE:    //38
        {
            m_creature->SetInCombatWithZone();
            break;
        }
        case ACTION_T_CALL_FOR_HELP:        //39
        {
            m_creature->CallForHelp((float)action.call_for_help.radius);
            break;
        }
        case ACTION_T_SET_SHEATH:           //40
        {
            m_creature->SetSheath(SheathState(action.set_sheath.sheath));
            break;
        }
        case ACTION_T_FORCE_DESPAWN:        //41
        {
            m_creature->ForcedDespawn(action.forced_despawn.msDelay);
            break;
        }
        case ACTION_T_SET_INVINCIBILITY_HP_LEVEL:       //42
        {
            if (action.invincibility_hp_level.is_percent)
                { m_InvinceabilityHpLevel = m_creature->GetMaxHealth() * action.invincibility_hp_level.hp_level / 100; }
            else
                { m_InvinceabilityHpLevel = action.invincibility_hp_level.hp_level; }
            break;
        }
        case ACTION_T_MOUNT_TO_ENTRY_OR_MODEL:          //43
        {
            if (action.mount.creatureId || action.mount.modelId)
            {
                // set model based on entry from creature_template
                if (action.mount.creatureId)
                {
                    if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(action.mount.creatureId))
                    {
                        uint32 display_id = Creature::ChooseDisplayId(cInfo);
                        m_creature->Mount(display_id);
                    }
                }
                // if no param1, then use value from param2 (modelId)
                else
                    { m_creature->Mount(action.mount.modelId); }
            }
            else
                { m_creature->Unmount(); }

            break;
        }
        case ACTION_T_THROW_AI_EVENT:                   //45
        {
            SendAIEventAround(AIEventType(action.throwEvent.eventType), pActionInvoker, 0, action.throwEvent.radius);
            break;
        }
        case ACTION_T_SET_THROW_MASK:                   //46
        {
            m_throwAIEventMask = action.setThrowMask.eventTypeMask;
            break;
        }
        case ACTION_T_SET_STAND_STATE:                  //47
        {
            m_creature->SetStandState(action.setStandState.standState);
            break;
        }
        case ACTION_T_CHANGE_MOVEMENT:                  //48
        {
            switch (action.changeMovement.movementType)
            {
                case IDLE_MOTION_TYPE:
                    m_creature->GetMotionMaster()->MoveIdle();
                    break;
                case RANDOM_MOTION_TYPE:
                    m_creature->GetMotionMaster()->MoveRandomAroundPoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), float(action.changeMovement.wanderDistance));
                    break;
                case WAYPOINT_MOTION_TYPE:
                    m_creature->GetMotionMaster()->MoveWaypoint();
                    break;
            }
            break;
        }
        case ACTION_T_SUMMON_UNIQUE:                    //49
        {
            Creature* pCreature;

            MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*m_creature, action.summon_unique.creatureId, true, false, 100, true);
            MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreature, u_check);
            Cell::VisitGridObjects(m_creature, searcher, 100);
            if (pCreature) { return; }

            Unit* target = GetTargetByType(action.summon_unique.target, pActionInvoker, pAIEventSender, reportTargetError);
            if (!target && reportTargetError)
                { sLog.outErrorEventAI("Event %u - NULL target for ACTION_T_SUMMON_UNIQUE(%u), target-type %u", EventId, action.type, action.summon_unique.target); }

            CreatureEventAI_Summon_Map::const_iterator i = sEventAIMgr.GetCreatureEventAISummonMap().find(action.summon_unique.spawnId);
            if (i == sEventAIMgr.GetCreatureEventAISummonMap().end())
            {
                sLog.outErrorEventAI("failed to spawn creature %u. Summon map index %u does not exist. EventID %d. CreatureID %d", action.summon_unique.creatureId, action.summon_unique.spawnId, EventId, m_creature->GetEntry());
                return;
            }

            if ((*i).second.SpawnTimeSecs)
                { pCreature = m_creature->SummonCreature(action.summon_unique.creatureId, (*i).second.position_x, (*i).second.position_y, (*i).second.position_z, (*i).second.orientation, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, (*i).second.SpawnTimeSecs); }
            else
                { pCreature = m_creature->SummonCreature(action.summon_unique.creatureId, (*i).second.position_x, (*i).second.position_y, (*i).second.position_z, (*i).second.orientation, TEMPSUMMON_TIMED_OOC_DESPAWN, 0); }

            if (!pCreature)
                { sLog.outErrorEventAI("failed to spawn creature %u. EventId %d.Creature %d", action.summon_unique.creatureId, EventId, m_creature->GetEntry()); }
            else if (action.summon_unique.target != TARGET_T_SELF && target)
                { pCreature->AI()->AttackStart(target); }

            break;
        }
        case ACTION_T_EMOTE_TARGET:         //50
        {
            Unit* pCreature = m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT, action.emoteTarget.targetGuid));
            if (!pCreature)
            {
                sLog.outErrorEventAI("Event %d. Cannot find creature by guid %d", EventId, action.emoteTarget.targetGuid);
                return;
            }


            m_creature->SetFacingToObject(pCreature);
            m_creature->HandleEmote(action.emoteTarget.emoteId);
            break;
        }
        default:
            break;
    }
}

void CreatureEventAI::JustRespawned()                       // NOTE that this is called from the AI's constructor as well
{
    Reset();

    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        // Reset generic timer
        if (i->Event.event_type == EVENT_T_TIMER_GENERIC)
        {
            if (i->UpdateRepeatTimer(m_creature, i->Event.timer.initialMin, i->Event.timer.initialMax))
                { i->Enabled = true; }
        }
        // Handle Spawned Events
        else if (SpawnedEventConditionsCheck(i->Event))
            { ProcessEvent(*i); }
    }
}

void CreatureEventAI::Reset()
{
    m_currSpell = 0;
    m_EventUpdateTime = EVENT_UPDATE_TIME;
    m_EventDiff = 0;
    m_throwAIEventStep = 0;

    // Reset all events to enabled
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        CreatureEventAI_Event const& event = i->Event;
        switch (event.event_type)
        {
                // Reset all out of combat timers
            case EVENT_T_TIMER_OOC:
            {
                if (i->UpdateRepeatTimer(m_creature, event.timer.initialMin, event.timer.initialMax))
                    i->Enabled = true;
                break;
            }
            default:
            // TODO: enable below code line / verify this is correct to enable events previously disabled (ex. aggro yell), instead of enable this in void Aggro()
            //i->Enabled = true;
            //i->Time = 0;
            break;
        }
    }
}

void CreatureEventAI::JustReachedHome()
{
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_REACHED_HOME)
            { ProcessEvent(*i); }
    }

    Reset();
}

void CreatureEventAI::EnterEvadeMode()
{
    m_creature->RemoveAllAurasOnEvade();
    m_creature->DeleteThreatList();
    m_creature->CombatStop(true);

    if (m_creature->IsAlive())
        { m_creature->GetMotionMaster()->MoveTargetedHome(); }

    m_creature->SetLootRecipient(NULL);

    // Handle Evade events
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_EVADE)
            { ProcessEvent(*i); }
    }
    m_creature->ResetPlayerDamageReq();
}

void CreatureEventAI::JustDied(Unit* killer)
{
    Reset();

    if (m_creature->IsGuard())
    {
        // Send Zone Under Attack message to the LocalDefense and WorldDefense Channels
        if (Player* pKiller = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
            { m_creature->SendZoneUnderAttackMessage(pKiller); }
    }

    if (m_throwAIEventMask & (1 << AI_EVENT_JUST_DIED))
        { SendAIEventAround(AI_EVENT_JUST_DIED, killer, 0, AIEVENT_DEFAULT_THROW_RADIUS); }

    // Handle On Death events
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_DEATH)
            { ProcessEvent(*i, killer); }
    }

    // reset phase after any death state events
    m_Phase = 0;
}

void CreatureEventAI::KilledUnit(Unit* victim)
{
    if (victim->GetTypeId() != TYPEID_PLAYER)
        { return; }

    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_KILL)
            { ProcessEvent(*i, victim); }
    }
}

void CreatureEventAI::JustSummoned(Creature* pUnit)
{
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_SUMMONED_UNIT)
            { ProcessEvent(*i, pUnit); }
    }
}

void CreatureEventAI::SummonedCreatureJustDied(Creature* pUnit)
{
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_SUMMONED_JUST_DIED)
            { ProcessEvent(*i, pUnit); }
    }
}

void CreatureEventAI::SummonedCreatureDespawn(Creature* pUnit)
{
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        if (i->Event.event_type == EVENT_T_SUMMONED_JUST_DESPAWN)
            { ProcessEvent(*i, pUnit); }
    }
}

void CreatureEventAI::ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* pInvoker, uint32 /*miscValue*/)
{
    MANGOS_ASSERT(pSender);

    for (CreatureEventAIList::iterator itr = m_CreatureEventAIList.begin(); itr != m_CreatureEventAIList.end(); ++itr)
    {
        if (itr->Event.event_type == EVENT_T_RECEIVE_AI_EVENT &&
            itr->Event.receiveAIEvent.eventType == eventType && (!itr->Event.receiveAIEvent.senderEntry || itr->Event.receiveAIEvent.senderEntry == pSender->GetEntry()))
            { ProcessEvent(*itr, pInvoker, pSender); }
    }
}

void CreatureEventAI::EnterCombat(Unit* enemy)
{
    // Check for on combat start events
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
    {
        CreatureEventAI_Event const& event = i->Event;
        switch (event.event_type)
        {
            case EVENT_T_AGGRO:
                i->Enabled = true;
                ProcessEvent(*i, enemy);
                break;
                // Reset all in combat timers
            case EVENT_T_TIMER_IN_COMBAT:
                if (i->UpdateRepeatTimer(m_creature, event.timer.initialMin, event.timer.initialMax))
                    { i->Enabled = true; }
                break;
                // All normal events need to be re-enabled and their time set to 0
            default:
                i->Enabled = true;
                i->Time = 0;
                break;
        }
    }

    m_EventUpdateTime = EVENT_UPDATE_TIME;
    m_EventDiff = 0;
}

void CreatureEventAI::AttackStart(Unit* who)
{
    if (!who)
        { return; }

    if (m_creature->Attack(who, m_MeleeEnabled))
    {
        m_creature->AddThreat(who);
        m_creature->SetInCombatWith(who);
        who->SetInCombatWith(m_creature);

        HandleMovementOnAttackStart(who);
    }
}

void CreatureEventAI::MoveInLineOfSight(Unit* who)
{
    if (!who)
        { return; }

    // Check for OOC LOS Event
    if (m_HasOOCLoSEvent && !m_creature->getVictim())
    {
        for (CreatureEventAIList::iterator itr = m_CreatureEventAIList.begin(); itr != m_CreatureEventAIList.end(); ++itr)
        {
            if (itr->Event.event_type == EVENT_T_OOC_LOS)
            {
                // can trigger if closer than fMaxAllowedRange
                float fMaxAllowedRange = (float)itr->Event.ooc_los.maxRange;

                // if friendly event && who is not hostile OR hostile event && who is hostile
                if ((itr->Event.ooc_los.noHostile && !m_creature->IsHostileTo(who)) ||
                    ((!itr->Event.ooc_los.noHostile) && m_creature->IsHostileTo(who)))
                {
                    // if range is ok and we are actually in LOS
                    if (m_creature->IsWithinDistInMap(who, fMaxAllowedRange) && m_creature->IsWithinLOSInMap(who))
                        { ProcessEvent(*itr, who); }
                }
            }
        }
    }

    if ((m_creature->GetCreatureInfo()->ExtraFlags & CREATURE_EXTRA_FLAG_NO_AGGRO) || m_creature->IsNeutralToAll())
        { return; }

    if (m_creature->CanInitiateAttack() && who->IsTargetableForAttack() &&
        m_creature->IsHostileTo(who) && who->isInAccessablePlaceFor(m_creature))
    {
        if (!m_creature->CanFly() && m_creature->GetDistanceZ(who) > CREATURE_Z_ATTACK_RANGE)
            { return; }

        float attackRadius = m_creature->GetAttackDistance(who);
        if (m_creature->IsWithinDistInMap(who, attackRadius) && m_creature->IsWithinLOSInMap(who))
        {
            if (!m_creature->getVictim())
            {
                AttackStart(who);
                who->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
            }
            else if (m_creature->GetMap()->IsDungeon())
            {
                m_creature->AddThreat(who);
                who->SetInCombatWith(m_creature);
            }
        }
    }
}

void CreatureEventAI::SpellHit(Unit* pUnit, const SpellEntry* pSpell)
{
    for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
        if (i->Event.event_type == EVENT_T_SPELLHIT)
            // If spell id matches (or no spell id) & if spell school matches (or no spell school)
            if (!i->Event.spell_hit.spellId || pSpell->Id == i->Event.spell_hit.spellId)
                if (GetSchoolMask(pSpell->School) & i->Event.spell_hit.schoolMask)
                    { ProcessEvent(*i, pUnit); }
}

void CreatureEventAI::UpdateAI(const uint32 diff)
{
    // Check if we are in combat (also updates calls threat update code)
    bool Combat = m_creature->SelectHostileTarget() && m_creature->getVictim();

    // Events are only updated once every EVENT_UPDATE_TIME ms to prevent lag with large amount of events
    if (m_EventUpdateTime < diff)
    {
        m_EventDiff += diff;

        // Check for time based events
        for (CreatureEventAIList::iterator i = m_CreatureEventAIList.begin(); i != m_CreatureEventAIList.end(); ++i)
        {
            // Decrement Timers
            if (i->Time)
            {
                if (i->Time > m_EventDiff)
                {
                    // Do not decrement timers if event cannot trigger in this phase
                    if (!(i->Event.event_inverse_phase_mask & (1 << m_Phase)))
                        i->Time -= m_EventDiff;
                }
                else
                    i->Time = 0;
            }

            // Skip processing of events that have time remaining or are disabled
            if (!(i->Enabled) || i->Time)
                continue;

            if (IsTimerBasedEvent(i->Event.event_type))
                ProcessEvent(*i);
        }

        m_EventDiff = 0;
        m_EventUpdateTime = EVENT_UPDATE_TIME;
    }
    else
    {
        m_EventDiff += diff;
        m_EventUpdateTime -= diff;
    }

    // Melee Auto-Attack (getVictim might be NULL as result of timer based events and actions)
    if (Combat && m_creature->getVictim() && m_MeleeEnabled)
        { DoMeleeAttackIfReady(); }
}

bool CreatureEventAI::IsVisible(Unit* pl) const
{
    return m_creature->IsWithinDist(pl, sWorld.getConfig(CONFIG_FLOAT_SIGHT_MONSTER))
           && pl->IsVisibleForOrDetect(m_creature, m_creature, true);
}

inline uint32 CreatureEventAI::GetRandActionParam(uint32 rnd, uint32 param1, uint32 param2, uint32 param3)
{
    switch (rnd % 3)
    {
        case 0: return param1;
        case 1: return param2;
        case 2: return param3;
    }
    return 0;
}

inline int32 CreatureEventAI::GetRandActionParam(uint32 rnd, int32 param1, int32 param2, int32 param3)
{
    switch (rnd % 3)
    {
        case 0: return param1;
        case 1: return param2;
        case 2: return param3;
    }
    return 0;
}

inline Unit* CreatureEventAI::GetTargetByType(uint32 Target, Unit* pActionInvoker, Creature* pAIEventSender, bool& isError, uint32 forSpellId, uint32 selectFlags)
{
    Unit* resTarget;
    switch (Target)
    {
        case TARGET_T_SELF:
            return m_creature;
        case TARGET_T_HOSTILE:
            resTarget = m_creature->getVictim();
            if (!resTarget)
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_SECOND_AGGRO:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 1, forSpellId, selectFlags);
            if (!resTarget && ((forSpellId == 0 && selectFlags == 0 && m_creature->GetThreatManager().getThreatList().size() > 1) || m_creature->GetThreatManager().getThreatList().empty()))
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_LAST_AGGRO:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_BOTTOMAGGRO, 0, forSpellId, selectFlags);
            if (!resTarget && m_creature->GetThreatManager().getThreatList().empty())
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_RANDOM:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, forSpellId, selectFlags);
            if (!resTarget && m_creature->GetThreatManager().getThreatList().empty())
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_RANDOM_NOT_TOP:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, forSpellId, selectFlags);
            if (!resTarget && ((forSpellId == 0 && selectFlags == 0 && m_creature->GetThreatManager().getThreatList().size() > 1) || m_creature->GetThreatManager().getThreatList().empty()))
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_RANDOM_PLAYER:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, forSpellId, SELECT_FLAG_PLAYER | selectFlags);
            if (!resTarget)
                { isError = true; }
            return resTarget;
        case TARGET_T_HOSTILE_RANDOM_NOT_TOP_PLAYER:
            resTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, forSpellId, SELECT_FLAG_PLAYER | selectFlags);
            if (!resTarget && ((forSpellId == 0 && selectFlags == 0 && m_creature->GetThreatManager().getThreatList().size() > 1) || m_creature->GetThreatManager().getThreatList().empty()))
                { isError = true; }
            return resTarget;
        case TARGET_T_ACTION_INVOKER:
            if (!pActionInvoker)
                { isError = true; }
            return pActionInvoker;
        case TARGET_T_ACTION_INVOKER_OWNER:
            resTarget = pActionInvoker ? pActionInvoker->GetCharmerOrOwnerOrSelf() : NULL;
            if (!resTarget)
                { isError = true; }
            return resTarget;
        case TARGET_T_EVENT_SENDER:
            if (!pAIEventSender)
                { isError = true; }
            return pAIEventSender;
        default:
            isError = true;
            return NULL;
    };
}

Unit* CreatureEventAI::DoSelectLowestHpFriendly(float range, uint32 MinHPDiff)
{
    Unit* pUnit = NULL;

    MaNGOS::MostHPMissingInRangeCheck u_check(m_creature, range, MinHPDiff);
    MaNGOS::UnitLastSearcher<MaNGOS::MostHPMissingInRangeCheck> searcher(pUnit, u_check);

    /*
    typedef TYPELIST_4(GameObject, Creature*except pets*, DynamicObject, Corpse*Bones*) AllGridObjectTypes;
    This means that if we only search grid then we can not possibly return pets or players so this is safe
    */
    Cell::VisitGridObjects(m_creature, searcher, range);
    return pUnit;
}

void CreatureEventAI::DoFindFriendlyCC(std::list<Creature*>& _list, float range)
{
    MaNGOS::FriendlyCCedInRangeCheck u_check(m_creature, range);
    MaNGOS::CreatureListSearcher<MaNGOS::FriendlyCCedInRangeCheck> searcher(_list, u_check);
    Cell::VisitGridObjects(m_creature, searcher, range);
}

void CreatureEventAI::DoFindFriendlyMissingBuff(std::list<Creature*>& _list, float range, uint32 spellid)
{
    MaNGOS::FriendlyMissingBuffInRangeCheck u_check(m_creature, range, spellid);
    MaNGOS::CreatureListSearcher<MaNGOS::FriendlyMissingBuffInRangeCheck> searcher(_list, u_check);
    Cell::VisitGridObjects(m_creature, searcher, range);
}

//*********************************
//*** Functions used globally ***

void CreatureEventAI::ReceiveEmote(Player* pPlayer, uint32 text_emote)
{
    for (CreatureEventAIList::iterator itr = m_CreatureEventAIList.begin(); itr != m_CreatureEventAIList.end(); ++itr)
    {
        if (itr->Event.event_type == EVENT_T_RECEIVE_EMOTE)
        {
            if (itr->Event.receive_emote.emoteId != text_emote)
                { continue; }

            PlayerCondition pcon(0, itr->Event.receive_emote.condition, itr->Event.receive_emote.conditionValue1, itr->Event.receive_emote.conditionValue2);
            if (pcon.Meets(pPlayer, m_creature->GetMap(), m_creature, CONDITION_FROM_EVENTAI))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "CreatureEventAI: ReceiveEmote CreatureEventAI: Condition ok, processing");
                ProcessEvent(*itr, pPlayer);
            }
        }
    }
}

#define HEALTH_STEPS 3

void CreatureEventAI::DamageTaken(Unit* dealer, uint32& damage)
{
    if (m_InvinceabilityHpLevel > 0 && m_creature->GetHealth() < m_InvinceabilityHpLevel + damage)
    {
        if (m_creature->GetHealth() <= m_InvinceabilityHpLevel)
            { damage = 0; }
        else
            { damage = m_creature->GetHealth() - m_InvinceabilityHpLevel; }
    }

    uint32 step = m_throwAIEventStep != 100 ? m_throwAIEventStep : 0;
    if (step < HEALTH_STEPS)
    {
        // Throw at 90%, 50% and 10% health
        float healthSteps[HEALTH_STEPS] = { 90.0f, 50.0f, 10.0f };
        float newHealthPercent = (m_creature->GetHealth() - damage) * 100.0f / m_creature->GetMaxHealth();
        AIEventType sendEvent[HEALTH_STEPS] = { AI_EVENT_LOST_SOME_HEALTH, AI_EVENT_LOST_HEALTH, AI_EVENT_CRITICAL_HEALTH };

        if (newHealthPercent > healthSteps[step])
            { return; }                                         // Not reached the next mark

        // search for highest reached mark (with actual event attached)
        for (uint32 i = HEALTH_STEPS - 1; i > step; --i)
        {
            if (newHealthPercent < healthSteps[i] && (m_throwAIEventMask & (1 << sendEvent[i])))
            {
                step = i;
                break;
            }
        }

        if (m_throwAIEventMask & (1 << sendEvent[step]))
            { SendAIEventAround(sendEvent[step], dealer, 0, AIEVENT_DEFAULT_THROW_RADIUS); }

        m_throwAIEventStep = step + 1;
    }
}

void CreatureEventAI::HealedBy(Unit* healer, uint32& healedAmount)
{
    if (m_throwAIEventStep == 100)
        { return; }

    if (m_creature->GetHealth() + healedAmount >= m_creature->GetMaxHealth())
    {
        if (m_throwAIEventMask & (1 << AI_EVENT_GOT_FULL_HEALTH))
            { SendAIEventAround(AI_EVENT_GOT_FULL_HEALTH, healer, 0, AIEVENT_DEFAULT_THROW_RADIUS); }
        m_throwAIEventStep = 100;
    }
}

bool CreatureEventAI::SpawnedEventConditionsCheck(CreatureEventAI_Event const& event)
{
    if (event.event_type != EVENT_T_SPAWNED)
        { return false; }

    switch (event.spawned.condition)
    {
        case SPAWNED_EVENT_ALWAY:
            // always
            return true;
        case SPAWNED_EVENT_MAP:
            // map ID check
            return m_creature->GetMapId() == event.spawned.conditionValue1;
        case SPAWNED_EVENT_ZONE:
        {
            // zone ID check
            uint32 zone, area;
            m_creature->GetZoneAndAreaId(zone, area);
            return zone == event.spawned.conditionValue1 || area == event.spawned.conditionValue1;
        }
        default:
            break;
    }

    return false;
}

void CreatureEventAI::OnSpellCastChange(const SpellEntry* pSpell, SpellCastResult reason)
{
    //sLog.outError("Received reason %u for %s, spell %u (curSpell = %u)", reason, m_creature->GetGuidStr().c_str(), pSpell->Id, m_currSpell);
    
    if (!pSpell || pSpell->Id != m_currSpell)
        return;

    m_currSpell = 0;

    switch (reason)
    {
        case SPELL_CAST_OK:
            break;
        // Add more cases here
        case SPELL_FAILED_OUT_OF_RANGE:
        case SPELL_FAILED_LINE_OF_SIGHT:
        case SPELL_FAILED_NO_POWER:
        case SPELL_FAILED_INTERRUPTED:
        case SPELL_FAILED_SILENCED:
            m_creature->CastStop();
            SetCombatMovementFlag(CM_SPELL);
            SetChase(true);
            return;
        default:
            m_creature->CastStop();
            break;
    }
    SetCombatMovementFlag(CM_SPELL, false);
    SetChase(false);

}

CanCastResult CreatureEventAI::DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags, ObjectGuid uiOriginalCasterGuid)
{
    SetCombatMovementFlag(CM_SPELL, false);
    m_currSpell = uiSpell;
    SetChase(false);

    CanCastResult result = CreatureAI::DoCastSpellIfCan(pTarget, uiSpell, uiCastFlags, uiOriginalCasterGuid);
    switch (result)
    {
        case CAST_OK:
            break;
        case CAST_FAIL_TOO_FAR:
        case CAST_FAIL_NO_LOS:
        case CAST_FAIL_POWER:
        case CAST_FAIL_SILENCED:
        {
            if (!(uiCastFlags & CAST_NO_MELEE_IF_OOM))
            {
                SetCombatMovementFlag(CM_SPELL);
                SetChase(true);
            }
            else
            {
                m_currSpell = 0;
            }
            break;
        }
        default:
            m_currSpell = 0;
            break;
    }

    return result;
}

