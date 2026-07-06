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

#include "PetMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Transports.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"

void PetMgr::LoadStableSlotsFromField(uint32 raw)
{
    m_stableSlots = raw;
    if (m_stableSlots > MAX_PET_STABLES)
    {
        sLog.outError("Player can have not more %u stable slots, but have in DB %u", MAX_PET_STABLES, uint32(m_stableSlots));
        m_stableSlots = MAX_PET_STABLES;
    }
}

void PetMgr::Remove(PetSaveMode mode)
{
    if (Pet* pet = m_owner->GetPet())
    {
        pet->Unsummon(mode, m_owner);
    }
}

void PetMgr::RemoveActionBar()
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << ObjectGuid();
    m_owner->SendDirectMessage(&data);
}

void PetMgr::UnsummonTemporaryIfAny()
{
    Pet* pet = m_owner->GetPet();
    if (!pet)
    {
        return;
    }

    if (!m_temporaryUnsummonedPetNumber && pet->isControlled() && !pet->isTemporarySummoned())
    {
        m_temporaryUnsummonedPetNumber = pet->GetCharmInfo()->GetPetNumber();
    }

    if (Transport* petTransport = pet->GetTransport())
    {
        petTransport->RemovePassenger(pet);
        pet->SetTransport(nullptr);
    }

    pet->Unsummon(PET_SAVE_AS_CURRENT, m_owner);
}

void PetMgr::ResummonTemporaryUnsummonedIfAny()
{
    if (!m_temporaryUnsummonedPetNumber)
    {
        return;
    }

    // not resummon in not appropriate state
    if (m_owner->IsPetNeedBeTemporaryUnsummoned())
    {
        return;
    }

    if (m_owner->GetPetGuid())
    {
        return;
    }

    Pet* NewPet = new Pet;
    if (!NewPet->LoadPetFromDB(m_owner, 0, m_temporaryUnsummonedPetNumber, true))
    {
        delete NewPet;
    }

    m_temporaryUnsummonedPetNumber = 0;
}
