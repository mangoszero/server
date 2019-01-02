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

#include "TemporarySummon.h"
#include "Log.h"
#include "CreatureAI.h"

TemporarySummon::TemporarySummon(ObjectGuid summoner) :
    Creature(CREATURE_SUBTYPE_TEMPORARY_SUMMON), m_type(TEMPSUMMON_TIMED_OOC_OR_CORPSE_DESPAWN), m_timer(0), m_lifetime(0), m_summoner(summoner)
{
}

void TemporarySummon::Update(uint32 update_diff,  uint32 diff)
{
    switch (m_type)
    {
        case TEMPSUMMON_MANUAL_DESPAWN:
            break;
        case TEMPSUMMON_TIMED_DESPAWN:
        {
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }

            m_timer -= update_diff;
            break;
        }
        case TEMPSUMMON_TIMED_OOC_DESPAWN:
        {
            if (!IsInCombat())
            {
                if (m_timer <= update_diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= update_diff;
            }
            else if (m_timer != m_lifetime)
                { m_timer = m_lifetime; }

            break;
        }

        case TEMPSUMMON_CORPSE_TIMED_DESPAWN:
        {
            if (IsCorpse())
            {
                if (m_timer <= update_diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= update_diff;
            }
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            break;
        }
        case TEMPSUMMON_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (IsDead())
            {
                UnSummon();
                return;
            }

            break;
        }
        case TEMPSUMMON_DEAD_DESPAWN:
        {
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            break;
        }
        case TEMPSUMMON_TIMED_OOC_OR_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (IsDead())
            {
                UnSummon();
                return;
            }

            if (!IsInCombat())
            {
                if (m_timer <= update_diff)
                {
                    UnSummon();
                    return;
                }
                else
                    { m_timer -= update_diff; }
            }
            else if (m_timer != m_lifetime)
                { m_timer = m_lifetime; }
            break;
        }
        case TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (IsDespawned())
            {
                UnSummon();
                return;
            }

            if (!IsInCombat() && IsAlive())
            {
                if (m_timer <= update_diff)
                {
                    UnSummon();
                    return;
                }
                else
                    { m_timer -= update_diff; }
            }
            else if (m_timer != m_lifetime)
                { m_timer = m_lifetime; }
            break;
        }
        case TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (IsDead())
            {
                UnSummon();
                return;
            }
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }
            m_timer -= update_diff;
            break;
        }
        case TEMPSUMMON_TIMED_OR_DEAD_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            if (m_timer <= update_diff)
            {
                UnSummon();
                return;
            }
            m_timer -= update_diff;
            break;
        }
    }

    switch (m_deathState)
    {
        case ALIVE:
        {
            if (GetCharmerGuid())
            {
                Unit* charmer = GetCharmer();
                if (!charmer || !IsWithinDistInMap(charmer, GetMap()->GetVisibilityDistance()))
                {
                    UnSummon();
                    return;
                }
            }
        }
    }

    Creature::Update(update_diff, diff);
}

void TemporarySummon::Summon(TempSummonType type, uint32 lifetime)
{
    m_type = type;
    m_timer = lifetime;
    m_lifetime = lifetime;

    GetMap()->Add((Creature*)this);
    AIM_Initialize();
}

void TemporarySummon::UnSummon()
{
    if (GetSummonerGuid().IsCreature())
        if (Creature* sum = GetMap()->GetCreature(GetSummonerGuid()))
            if (sum->AI())
                { sum->AI()->SummonedCreatureDespawn(this); }

    AddObjectToRemoveList();
}

void TemporarySummon::SaveToDB()
{
}

void TemporarySummon::RemoveFromWorld()
{
    if (IsInWorld())
    {
        Unit* charmer = GetCharmer();
        if (charmer && charmer->GetCharmGuid() == GetObjectGuid())
        {
            charmer->Uncharm();
            if (charmer->GetCharmGuid() == GetObjectGuid() && charmer->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)charmer;
                Camera& camera = player->GetCamera();
                player->InterruptSpell(CURRENT_CHANNELED_SPELL);

                player->SetClientControl(player, 1);
                player->SetMover(NULL);
                camera.ResetView();
                player->RemovePetActionBar();
            }
        }
    }
    Creature::RemoveFromWorld();
}

TemporarySummonWaypoint::TemporarySummonWaypoint(ObjectGuid summoner, uint32 waypoint_id, int32 path_id, uint32 pathOrigin) :
    TemporarySummon(summoner),
    m_waypoint_id(waypoint_id),
    m_path_id(path_id),
    m_pathOrigin(pathOrigin)
{
}
