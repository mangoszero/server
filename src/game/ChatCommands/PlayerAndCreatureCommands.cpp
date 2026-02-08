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

#include "Chat.h"
#include "ObjectMgr.h"
#include "PathFinder.h"
#include "TargetedMovementGenerator.h"
#include "MovementGenerator.h"
#include "FollowerReference.h"
#include "G3D/Vector3.h"

 /**********************************************************************
      CommandTable : commandTable
 /***********************************************************************/

// demorph player or unit
bool ChatHandler::HandleDeMorphCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        target = m_session->GetPlayer();
    }


    // check online security
    else if (target->GetTypeId() == TYPEID_PLAYER && HasLowerSecurity((Player*)target))
    {
        return false;
    }

    target->DeMorph();

    return true;
}

// morph creature or player
bool ChatHandler::HandleModifyMorphCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    uint32 display_id = (uint32)atoi(args);

    CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(display_id);
    if (!displayEntry)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* target = getSelectedUnit();
    if (!target)
    {
        target = m_session->GetPlayer();
    }

    // check online security
    else if (target->GetTypeId() == TYPEID_PLAYER && HasLowerSecurity((Player*)target))
    {
        return false;
    }

    target->SetDisplayId(display_id);

    return true;
}

bool ChatHandler::HandleDamageCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!target->IsAlive())
    {
        return true;
    }

    int32 damage_int;
    if (!ExtractInt32(&args, damage_int))
    {
        return false;
    }

    if (damage_int <= 0)
    {
        return true;
    }

    uint32 damage = damage_int;

    // For console, use target as damage dealer; for in-game, use session player
    Player* player = m_session ? m_session->GetPlayer() : nullptr;

    // flat melee damage without resistance/etc reduction
    if (!*args)
    {
        if (player)
        {
            player->DealDamage(target, damage, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            if (target != player)
            {
                player->SendAttackStateUpdate(HITINFO_NORMALSWING2, target, SPELL_SCHOOL_MASK_NORMAL, damage, 0, 0, VICTIMSTATE_NORMAL, 0);
            }
        }
        else
        {
            // Console: target damages itself (environmental-style)
            target->DealDamage(target, damage, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        }
        return true;
    }

    uint32 school;
    if (!ExtractUInt32(&args, school))
    {
        return false;
    }

    if (school >= MAX_SPELL_SCHOOL)
    {
        return false;
    }

    SpellSchoolMask schoolmask = GetSchoolMask(school);

    if (player && (schoolmask & SPELL_SCHOOL_MASK_NORMAL))
    {
        damage = player->CalcArmorReducedDamage(target, damage);
    }

    // melee damage by specific school
    if (!*args)
    {
        uint32 absorb = 0;
        uint32 resist = 0;

        if (player)
        {
            target->CalculateDamageAbsorbAndResist(player, schoolmask, SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);

            if (damage <= absorb + resist)
            {
                return true;
            }

            damage -= absorb + resist;

            player->DealDamageMods(target, damage, &absorb);
            player->DealDamage(target, damage, NULL, DIRECT_DAMAGE, schoolmask, NULL, false);
            player->SendAttackStateUpdate(HITINFO_NORMALSWING2, target, schoolmask, damage, absorb, resist, VICTIMSTATE_NORMAL, 0);
        }
        else
        {
            // Console: simplified damage without player-specific calculations
            target->DealDamage(target, damage, NULL, DIRECT_DAMAGE, schoolmask, NULL, false);
        }
        return true;
    }

    // non-melee damage
    uint32 spellid = ExtractSpellIdFromLink(&args);
    if (!spellid || !sSpellStore.LookupEntry(spellid))
    {
        return false;
    }

    if (player)
    {
        player->SpellNonMeleeDamageLog(target, spellid, damage);
    }
    else
    {
        // Console: spell damage not supported without a caster
        SendSysMessage("Spell damage requires an in-game player.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}


bool ChatHandler::HandleDieCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (HasLowerSecurity((Player*)target, ObjectGuid(), false))
        {
            return false;
        }
    }

    if (target->IsAlive())
    {
        if (m_session)
        {
            // In-game: player deals the damage
            m_session->GetPlayer()->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        }
        else
        {
            // Console: use environmental/direct kill
            target->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        }
    }

    return true;
}

bool ChatHandler::HandleMovegensCommand(char* /*args*/)
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_MOVEGENS_LIST, (unit->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature"), unit->GetGUIDLow());

    MotionMaster* mm = unit->GetMotionMaster();
    float x, y, z;
    mm->GetDestination(x, y, z);
    for (MotionMaster::const_iterator itr = mm->begin(); itr != mm->end(); ++itr)
    {
        switch ((*itr)->GetMovementGeneratorType())
        {
            case IDLE_MOTION_TYPE:          SendSysMessage(LANG_MOVEGENS_IDLE);          break;
            case RANDOM_MOTION_TYPE:        SendSysMessage(LANG_MOVEGENS_RANDOM);        break;
            case WAYPOINT_MOTION_TYPE:      SendSysMessage(LANG_MOVEGENS_WAYPOINT);      break;
            case CONFUSED_MOTION_TYPE:      SendSysMessage(LANG_MOVEGENS_CONFUSED);      break;

            case CHASE_MOTION_TYPE:
            {
                Unit* target;
                if (unit->GetTypeId() == TYPEID_PLAYER)
                {
                    target = static_cast<ChaseMovementGenerator<Player> const*>(*itr)->GetTarget();
                }
                else
                {
                    target = static_cast<ChaseMovementGenerator<Creature> const*>(*itr)->GetTarget();
                }

                if (!target)
                {
                    SendSysMessage(LANG_MOVEGENS_CHASE_NULL);
                }
                else if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    PSendSysMessage(LANG_MOVEGENS_CHASE_PLAYER, target->GetName(), target->GetGUIDLow());
                }
                else
                {
                    PSendSysMessage(LANG_MOVEGENS_CHASE_CREATURE, target->GetName(), target->GetGUIDLow());
                }
                break;
            }
            case FOLLOW_MOTION_TYPE:
            {
                Unit* target;
                if (unit->GetTypeId() == TYPEID_PLAYER)
                {
                    target = static_cast<FollowMovementGenerator<Player> const*>(*itr)->GetTarget();
                }
                else
                {
                    target = static_cast<FollowMovementGenerator<Creature> const*>(*itr)->GetTarget();
                }

                if (!target)
                {
                    SendSysMessage(LANG_MOVEGENS_FOLLOW_NULL);
                }
                else if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    PSendSysMessage(LANG_MOVEGENS_FOLLOW_PLAYER, target->GetName(), target->GetGUIDLow());
                }
                else
                {
                    PSendSysMessage(LANG_MOVEGENS_FOLLOW_CREATURE, target->GetName(), target->GetGUIDLow());
                }
                break;
            }
            case HOME_MOTION_TYPE:
                if (unit->GetTypeId() == TYPEID_UNIT)
                {
                    PSendSysMessage(LANG_MOVEGENS_HOME_CREATURE, x, y, z);
                }
                else
                {
                    SendSysMessage(LANG_MOVEGENS_HOME_PLAYER);
                }
                break;
            case FLIGHT_MOTION_TYPE:   SendSysMessage(LANG_MOVEGENS_FLIGHT);  break;
            case POINT_MOTION_TYPE:
            {
                PSendSysMessage(LANG_MOVEGENS_POINT, x, y, z);
                break;
            }
            case FLEEING_MOTION_TYPE:  SendSysMessage(LANG_MOVEGENS_FEAR);    break;
            case DISTRACT_MOTION_TYPE: SendSysMessage(LANG_MOVEGENS_DISTRACT);  break;
            case EFFECT_MOTION_TYPE: SendSysMessage(LANG_MOVEGENS_EFFECT);  break;
            default:
                PSendSysMessage(LANG_MOVEGENS_UNKNOWN, (*itr)->GetMovementGeneratorType());
                break;
        }
    }
    return true;
}

bool ChatHandler::HandleSetViewCommand(char* /*args*/)
{
    if (Unit* unit = getSelectedUnit())
    {
        m_session->GetPlayer()->GetCamera().SetView(unit);
    }
    else
    {
        PSendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}
