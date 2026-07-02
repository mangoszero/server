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

#include "Pet.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "Transports.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"

// numbers represent minutes * 100 while happy (you get 100 loyalty points per min while happy)
uint32 const LevelUpLoyalty[6] =
{
    5500,
    11500,
    17000,
    23500,
    31000,
    39500,
};

uint32 const LevelStartLoyalty[6] =
{
    2000,
    4500,
    7000,
    10000,
    13500,
    17500,
};

/**
 * @brief Creates a pet instance of the specified type.
 *
 * @param type The pet type to initialize.
 */
Pet::Pet(PetType type) : Creature(CREATURE_SUBTYPE_PET),
    m_TrainingPoints(0), m_resetTalentsCost(0), m_resetTalentsTime(0),
    m_removed(false), m_pendingTransportReboard(false), m_transport(nullptr), m_happinessTimer(7500), m_loyaltyTimer(12000), m_petType(type), m_duration(0),
    m_loyaltyPoints(0), m_bonusdamage(0), m_auraUpdateMask(0), m_loading(false),
    m_petModeFlags(PET_MODE_DEFAULT)
{
    m_name = "Pet";
    m_regenTimer = 4000;

    // pets always have a charminfo, even if they are not actually charmed
    CharmInfo* charmInfo = InitCharmInfo(this);

    if (type == MINI_PET)                                   // always passive
    {
        charmInfo->SetReactState(REACT_PASSIVE);
    }
    else if (type == GUARDIAN_PET)                          // always aggressive
    {
        charmInfo->SetReactState(REACT_AGGRESSIVE);
    }
}

/**
 * @brief Destroys the pet instance.
 */
Pet::~Pet()
{
}

/**
 * @brief Adds the pet to the world and object store.
 */
void Pet::AddToWorld()
{
    ///- Register the pet for guid lookup
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<Pet>(GetObjectGuid(), (Pet*)this);
    }

    Unit::AddToWorld();
}

/**
 * @brief Removes the pet from the world and object store.
 */
void Pet::RemoveFromWorld()
{
    ///- Remove the pet from the accessor
    if (IsInWorld())
    {
        GetMap()->GetObjectsStore().erase<Pet>(GetObjectGuid(), (Pet*)NULL);
    }

    ///- Don't call the function for Creature, normal mobs + totems go in a different storage
    Unit::RemoveFromWorld();
}




/**
 * @brief Updates the pet death state and related pet-specific behavior.
 *
 * @param s The new death state.
 */
void Pet::SetDeathState(DeathState s)                       // overwrite virtual Creature::SetDeathState and Unit::SetDeathState
{
    Creature::SetDeathState(s);
    if (GetDeathState() == CORPSE)
    {
        // remove summoned pet (no corpse)
        if (getPetType() != SUMMON_PET)
        {
            // pet corpse non lootable and non skinnable
            SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
            RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

            // lose happiness when died and not in BG
            MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
            if (!mapEntry || (mapEntry->map_type != MAP_BATTLEGROUND))
            {
                ModifyPower(POWER_HAPPINESS, -HAPPINESS_LEVEL_SIZE);
            }

            SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
        }
    }
    else if (GetDeathState() == ALIVE)
    {
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
        CastPetAuras(true);
    }
    CastOwnerTalentAuras();
}

/**
 * @brief Updates the pet each server tick.
 *
 * @param update_diff The elapsed time since the last update in milliseconds.
 * @param diff The world update time forwarded to base update logic.
 */
void Pet::Update(uint32 update_diff, uint32 diff)
{
    if (m_removed)                                          // pet already removed, just wait in remove queue, no updates
    {
        return;
    }

    switch (m_deathState)
    {
        case CORPSE:
        {
            if (getPetType() != HUNTER_PET || m_corpseRemoveTime <= time(NULL))
            {
                Unsummon(PET_SAVE_NOT_IN_SLOT);
                return;
            }
            break;
        }
        case ALIVE:
        {
            // unsummon pet that lost owner
            Unit* owner = GetOwner();
            if (!owner ||
                (!m_transport && !IsWithinDistInMap(owner, GetMap()->GetVisibilityDistance()) && (owner->GetCharmGuid() && (owner->GetCharmGuid() != GetObjectGuid()))) ||
                (isControlled() && !owner->GetPetGuid()))
            {
                Unsummon(PET_SAVE_REAGENTS);
                return;
            }

            if (isControlled())
            {
                if (owner->GetPetGuid() != GetObjectGuid())
                {
                    Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }

            if (m_duration > 0)
            {
                if (m_duration > (int32)update_diff)
                {
                    m_duration -= (int32)update_diff;
                }
                else
                {
                    Unsummon(getPetType() != SUMMON_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }

            if (Player* plOwner = owner->ToPlayer())
            {
                UpdateTransport(plOwner);
            }

            break;
        }
        default:
            break;
    }

    Creature::Update(update_diff, diff);
}

/**
 * @brief Synchronizes pet transport boarding state with its owner.
 *
 * @param plOwner The owning player.
 */
void Pet::UpdateTransport(Player* plOwner)
{
    Transport* tr = plOwner->GetTransport();

    // Disembark if the owner has left the transport but the pet hasn't yet.
    if (m_transport && tr != m_transport)
    {
        m_transport->RemovePassenger(this);
        m_transport = nullptr;
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
        m_movementInfo.ClearTransportData();
        NearTeleportTo(plOwner->GetPositionX(), plOwner->GetPositionY(),
            plOwner->GetPositionZ(), plOwner->GetOrientation());
    }
    else // Board pet onto transport once it has swum/walked close enough to the owner.
    if (!m_transport && tr)
    {
        float dx = GetPositionX() - plOwner->GetPositionX();
        float dy = GetPositionY() - plOwner->GetPositionY();
        float dist2d = sqrt(dx*dx + dy*dy);
        if (dist2d <= 6.0f)
        {
            tr->AddPassenger(this);
            m_transport = tr;
            Position const* tpos = plOwner->m_movementInfo.GetTransportPos();
            m_movementInfo.SetTransportData(tr->GetObjectGuid(), tpos->x, tpos->y, tpos->z, tpos->o, 0);
            m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
            m_movementInfo.ChangePosition(plOwner->GetPositionX(), plOwner->GetPositionY(), plOwner->GetPositionZ(), plOwner->GetOrientation());
            GetMotionMaster()->Clear(false);
            if (GetCharmInfo())
            {
                GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);
            }

            NearTeleportTo(plOwner->GetPositionX(), plOwner->GetPositionY(), plOwner->GetPositionZ(), plOwner->GetOrientation());
            if (movespline)
            {
                WorldPacket moveData(SMSG_MONSTER_MOVE_TRANSPORT, 64);
                moveData << GetPackGUID();
                moveData << tr->GetPackGUID();
                moveData << tpos->x << tpos->y << tpos->z;
                moveData << movespline->GetId();
                moveData << uint8(Movement::MonsterMoveStop);
                SendMessageToSet(&moveData, true);
            }
            SendCreateUpdateToPlayer(plOwner);
        }
    }

    if (m_pendingTransportReboard)
    {
        m_pendingTransportReboard = false;
        plOwner->PetSpellInitialize();
        if (tr && movespline)
        {
            Position const* tpos = m_movementInfo.GetTransportPos();
            WorldPacket moveData(SMSG_MONSTER_MOVE_TRANSPORT, 64);
            moveData << GetPackGUID();
            moveData << tr->GetPackGUID();
            moveData << tpos->x << tpos->y << tpos->z;
            moveData << movespline->GetId();
            moveData << uint8(Movement::MonsterMoveStop);
            plOwner->SendDirectMessage(&moveData);
        }
        SendCreateUpdateToPlayer(plOwner);
    }
}

/**
 * @brief Handles follow movement while the owner is on a transport.
 *
 * @param target The unit being followed.
 * @param offset The follow distance offset.
 * @param angle The follow angle.
 * @param walking true to use walking speed.
 * @param outMoved Receives whether a movement update was launched.
 * @return true if transport-specific handling was applied; otherwise, false.
 */
bool Pet::HandleTransportFollow(Unit* target, float offset, float angle, bool walking, bool& outMoved)
{
    outMoved = false;
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
    {
        return false;
    }

    Transport* masterTransport = static_cast<Player*>(target)->GetTransport();
    if (!masterTransport)
    {
        return false;
    }

    if (m_transport == masterTransport)
    {
        // Both on the same transport — move in transport-relative space.
        float tx = masterTransport->GetPositionX();
        float ty = masterTransport->GetPositionY();
        float tz = masterTransport->GetPositionZ();
        float to = masterTransport->GetOrientation();
        float cos_o = cos(to), sin_o = sin(to);

        Position const* curRelPos = m_movementInfo.GetTransportPos();
        float startRelX = curRelPos->x, startRelY = curRelPos->y, startRelZ = curRelPos->z;

        Position const* masterTPos = target->m_movementInfo.GetTransportPos();
        float followDist = offset + GetObjectBoundingRadius() + target->GetObjectBoundingRadius();
        float destRelX = masterTPos->x + cos(angle) * followDist;
        float destRelY = masterTPos->y + sin(angle) * followDist;
        float destRelZ = masterTPos->z;

        float relDx = destRelX - startRelX, relDy = destRelY - startRelY, relDz = destRelZ - startRelZ;
        float relDist = sqrt(relDx * relDx + relDy * relDy + relDz * relDz);
        if (relDist < 0.5f)
        {
            return true;
        }

        m_movementInfo.SetTransportData(masterTransport->GetObjectGuid(), destRelX, destRelY, destRelZ, 0.0f, 0);

        float speed = GetSpeed(walking ? MOVE_WALK : MOVE_RUN);
        uint32 durationMs = (speed > 0.0f) ? uint32(relDist / speed * 1000.0f) : 0;

        float destWX = tx + cos_o * destRelX - sin_o * destRelY;
        float destWY = ty + sin_o * destRelX + cos_o * destRelY;
        float destWZ = tz + destRelZ;
        GetMap()->CreatureRelocation(this, destWX, destWY, destWZ, GetOrientation());

        if (durationMs > 0)
        {
            WorldPacket moveTransport(SMSG_MONSTER_MOVE_TRANSPORT, 80);
            moveTransport << GetPackGUID();
            moveTransport << masterTransport->GetPackGUID();
            moveTransport << startRelX << startRelY << startRelZ;
            moveTransport << movespline->GetId();
            moveTransport << uint8(Movement::MonsterMoveNormal);
            moveTransport << uint32(Movement::MoveSplineFlag::Runmode);
            moveTransport << durationMs;
            moveTransport << uint32(0);
            moveTransport << destRelX << destRelY << destRelZ;
            SendMessageToSet(&moveTransport, true);
        }

        outMoved = true;
        return true;
    }

    // Master is on a transport the pet hasn't boarded yet — approach in world space.
    float tx, ty, tz;
    target->GetPosition(tx, ty, tz);
    Movement::MoveSplineInit init(*this);
    init.MoveTo(tx, ty, tz);
    init.SetWalk(walking);
    init.Launch();
    outMoved = true;
    return true;
}

/**
 * @brief Regenerates pet health, power, happiness, and loyalty timers.
 *
 * @param update_diff The elapsed time since the last update in milliseconds.
 */
void Pet::RegenerateAll(uint32 update_diff)
{
    // regenerate focus
    if (m_regenTimer <= update_diff)
    {
        if (!IsInCombat() || IsPolymorphed())
        {
            RegenerateHealth();
        }

        RegeneratePower();

        m_regenTimer = 4000;
    }
    else
    {
        m_regenTimer -= update_diff;
    }

    if (getPetType() != HUNTER_PET)
    {
        return;
    }

    if (m_happinessTimer <= update_diff)
    {
        LooseHappiness();
        m_happinessTimer = 7500;
    }
    else
    {
        m_happinessTimer -= update_diff;
    }

    if (m_loyaltyTimer <= update_diff)
    {
        TickLoyaltyChange();
        m_loyaltyTimer = 12000;
    }
    else
    {
        m_loyaltyTimer -= update_diff;
    }
}

/**
 * @brief Decreases hunter pet happiness over time.
 */
void Pet::LooseHappiness()
{
    uint32 curValue = GetPower(POWER_HAPPINESS);
    if (curValue <= 0)
    {
        return;
    }
    int32 addvalue = (140 >> GetLoyaltyLevel()) * 125;      // value is 70/35/17/8/4 (per min) * 1000 / 8 (timer 7.5 secs)
    if (IsInCombat())                                       // we know in combat happiness fades faster, multiplier guess
    {
        addvalue = int32(addvalue * 1.5);
    }
    ModifyPower(POWER_HAPPINESS, -addvalue);
}

/**
 * @brief Modifies hunter pet loyalty points and rank transitions.
 *
 * @param addvalue The loyalty delta to apply.
 */
void Pet::ModifyLoyalty(int32 addvalue)
{
    uint32 loyaltylevel = GetLoyaltyLevel();

    if (addvalue > 0)                                       // only gain influenced, not loss
    {
        addvalue = int32((float)addvalue * sWorld.getConfig(CONFIG_FLOAT_RATE_LOYALTY));
    }

    if (loyaltylevel >= BEST_FRIEND && (addvalue + m_loyaltyPoints) > int32(GetMaxLoyaltyPoints(loyaltylevel)))
    {
        return;
    }

    m_loyaltyPoints += addvalue;

    if (m_loyaltyPoints < 0)
    {
        if (loyaltylevel > REBELLIOUS)
        {
            // level down
            --loyaltylevel;
            SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
            m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
            SetTP(m_TrainingPoints - int32(getLevel()));
        }
        else
        {
            m_loyaltyPoints = 0;
            Unit* owner = GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            {
                WorldPacket data(SMSG_PET_BROKEN, 0);
                ((Player*)owner)->GetSession()->SendPacket(&data);

                // run away
                Unsummon(PET_SAVE_AS_DELETED, owner);
            }
        }
    }
    // level up
    else if (m_loyaltyPoints > int32(GetMaxLoyaltyPoints(loyaltylevel)))
    {
        ++loyaltylevel;
        SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
        m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
        SetTP(m_TrainingPoints + getLevel());
    }
}

/**
 * @brief Applies the periodic loyalty change based on the pet's happiness state.
 */
void Pet::TickLoyaltyChange()
{
    int32 addvalue;

    switch (GetHappinessState())
    {
        case HAPPY:   addvalue =  20; break;
        case CONTENT: addvalue =  10; break;
        case UNHAPPY: addvalue = -20; break;
        default:
            return;
    }
    ModifyLoyalty(addvalue);
}

/**
 * @brief Grants a loyalty bonus after a kill.
 *
 * @param level The defeated creature level.
 */
void Pet::KillLoyaltyBonus(uint32 level)
{
    if (level > 100)
    {
        return;
    }

    // at lower levels gain is faster | the lower loyalty the more loyalty is gained
    uint32 bonus = uint32(((100 - level) / 10) + (6 - GetLoyaltyLevel()));
    ModifyLoyalty(bonus);
}

/**
 * @brief Gets the current hunter pet happiness state.
 *
 * @return The happiness state.
 */
HappinessState Pet::GetHappinessState()
{
    if (GetPower(POWER_HAPPINESS) < HAPPINESS_LEVEL_SIZE)
    {
        return UNHAPPY;
    }
    else if (GetPower(POWER_HAPPINESS) >= HAPPINESS_LEVEL_SIZE * 2)
    {
        return HAPPY;
    }
    else
    {
        return CONTENT;
    }
}

/**
 * @brief Sets the hunter pet loyalty level field.
 *
 * @param level The loyalty level to store.
 */
void Pet::SetLoyaltyLevel(LoyaltyLevel level)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 1, level);
}

/**
 * @brief Checks whether the pet can learn another active spell family.
 *
 * @param spellid The spell being evaluated.
 * @return true if another active spell can be learned; otherwise, false.
 */
bool Pet::CanTakeMoreActiveSpells(uint32 spellid)
{
    uint8  activecount = 1;
    uint32 chainstartstore[ACTIVE_SPELLS_MAX];

    if (IsPassiveSpell(spellid))
    {
        return true;
    }

    chainstartstore[0] = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        if (IsPassiveSpell(itr->first))
        {
            continue;
        }

        uint32 chainstart = sSpellMgr.GetFirstSpellInChain(itr->first);

        uint8 x;

        for (x = 0; x < activecount; ++x)
        {
            if (chainstart == chainstartstore[x])
            {
                break;
            }
        }

        if (x == activecount)                               // spellchain not yet saved -> add active count
        {
            ++activecount;
            if (activecount > ACTIVE_SPELLS_MAX)
            {
                return false;
            }
            chainstartstore[x] = chainstart;
        }
    }
    return true;
}

/**
 * @brief Checks whether the pet has enough training points for a spell.
 *
 * @param spellid The spell being evaluated.
 * @return true if the pet can afford the spell; otherwise, false.
 */
bool Pet::HasTPForSpell(uint32 spellid)
{
    int32 neededtrainp = GetTPForSpell(spellid);
    if ((m_TrainingPoints - neededtrainp < 0 || neededtrainp < 0) && neededtrainp != 0)
    {
        return false;
    }
    return true;
}

/**
 * @brief Computes the net training point cost for learning a spell.
 *
 * @param spellid The spell to evaluate.
 * @return The training point cost delta.
 */
int32 Pet::GetTPForSpell(uint32 spellid)
{
    uint32 basetrainp = 0;

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spellid);
    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        if (!_spell_idx->second->reqtrainpoints)
        {
            return 0;
        }

        basetrainp = _spell_idx->second->reqtrainpoints;
        break;
    }

    uint32 spenttrainp = 0;
    uint32 chainstart = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        if (sSpellMgr.GetFirstSpellInChain(itr->first) == chainstart)
        {
            SkillLineAbilityMapBounds _bounds = sSpellMgr.GetSkillLineAbilityMapBounds(itr->first);

            for (SkillLineAbilityMap::const_iterator _spell_idx2 = _bounds.first; _spell_idx2 != _bounds.second; ++_spell_idx2)
            {
                if (_spell_idx2->second->reqtrainpoints > spenttrainp)
                {
                    spenttrainp = _spell_idx2->second->reqtrainpoints;
                    break;
                }
            }
        }
    }

    return int32(basetrainp) - int32(spenttrainp);
}

/**
 * @brief Gets the maximum loyalty points for a loyalty rank.
 *
 * @param level The loyalty rank.
 * @return The maximum loyalty points for that rank.
 */
uint32 Pet::GetMaxLoyaltyPoints(uint32 level)
{
    if (level < 1)
    {
        level = 1;  // prevent SIGSEGV (out of range)
    }
    if (level > 6)
    {
        level = 6;  // prevent SIGSEGV (out of range)
    }
    return LevelUpLoyalty[level - 1];
}

/**
 * @brief Gets the starting loyalty points for a loyalty rank.
 *
 * @param level The loyalty rank.
 * @return The starting loyalty points for that rank.
 */
uint32 Pet::GetStartLoyaltyPoints(uint32 level)
{
    if (level < 1)
    {
        level = 1;  // prevent SIGSEGV (out of range)
    }
    if (level > 6)
    {
        level = 6;  // prevent SIGSEGV (out of range)
    }
    return LevelStartLoyalty[level - 1];
}

/**
 * @brief Sets the pet training points and updates the display field.
 *
 * @param TP The new training point value.
 */
void Pet::SetTP(int32 TP)
{
    m_TrainingPoints = TP;
    SetUInt32Value(UNIT_TRAINING_POINTS, (uint32)GetDispTP());
}

/**
 * @brief Gets the client-visible training point display value.
 *
 * @return The displayed training point value.
 */
int32 Pet::GetDispTP()
{
    if (getPetType() != HUNTER_PET)
    {
        return(0);
    }
    if (m_TrainingPoints < 0)
    {
        return -m_TrainingPoints;
    }
    else
    {
        return -(m_TrainingPoints + 1);
    }
}

/**
 * @brief Unsummons the pet and optionally saves it.
 *
 * @param mode The pet save mode to use.
 * @param owner Optional owner override.
 */
void Pet::Unsummon(PetSaveMode mode, Unit* owner /*= NULL*/)
{
    if (!owner)
    {
        owner = GetOwner();
    }

    CombatStop();

    if (owner)
    {
        if (GetOwnerGuid() != owner->GetObjectGuid())
        {
            return;
        }

        Player* p_owner = owner->GetTypeId() == TYPEID_PLAYER ? (Player*)owner : NULL;

        if (p_owner)
        {
            // not save secondary permanent pet as current
            if (mode == PET_SAVE_AS_CURRENT && p_owner->GetTemporaryUnsummonedPetNumber() &&
                p_owner->GetTemporaryUnsummonedPetNumber() != GetCharmInfo()->GetPetNumber())
            {
                mode = PET_SAVE_NOT_IN_SLOT;
            }

            if (mode == PET_SAVE_REAGENTS)
            {
                // returning of reagents only for players, so best done here
                uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL);
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

                if (spellInfo)
                {
                    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                    {
                        if (spellInfo->Reagent[i] > 0)
                        {
                            ItemPosCountVec dest;           // for succubus, voidwalker, felhunter and felguard credit soulshard when despawn reason other than death (out of range, logout)
                            uint8 msg = p_owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->Reagent[i], spellInfo->ReagentCount[i]);
                            if (msg == EQUIP_ERR_OK)
                            {
                                Item* item = p_owner->StoreNewItem(dest, spellInfo->Reagent[i], true);
                                if (p_owner->IsInWorld())
                                {
                                    p_owner->SendNewItem(item, spellInfo->ReagentCount[i], true, false);
                                }
                            }
                        }
                    }
                }
            }

            if (isControlled())
            {
                p_owner->RemovePetActionBar();

                if (p_owner->GetGroup())
                {
                    p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
                }
            }
        }

        // only if current pet in slot
        switch (getPetType())
        {
            case MINI_PET:
                if (p_owner)
                {
                    p_owner->_SetMiniPet(NULL);
                }
                break;
            case GUARDIAN_PET:
                owner->RemoveGuardian(this);
                break;
            default:
                if (owner->GetPetGuid() == GetObjectGuid())
                {
                    owner->SetPet(NULL);
                }
                break;
        }

        if (m_transport)
        {
            m_transport->RemovePassenger(this);
            m_transport = nullptr;
        }
    }
    else if (m_transport)
    {
        m_transport->RemovePassenger(this);
        m_transport = nullptr;
    }

    SavePetToDB(mode);
    AddObjectToRemoveList();
    m_removed = true;
}

/**
 * @brief Grants pet experience and handles leveling.
 *
 * @param xp The raw experience amount.
 */
void Pet::GivePetXP(uint32 xp)
{
    xp *= sWorld.getConfig(CONFIG_FLOAT_RATE_XP_PETKILL);
    if (getPetType() != HUNTER_PET)
    {
        return;
    }

    if (xp < 1)
    {
        return;
    }

    if (!IsAlive())
    {
        return;
    }

    uint32 level = getLevel();
    uint32 maxlevel = std::min(sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL), GetOwner()->getLevel());

    // pet not receive xp for level equal to owner level
    if (level >= maxlevel)
    {
        return;
    }

    uint32 nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    uint32 curXP = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
    uint32 newXP = curXP + xp;

    while (newXP >= nextLvlXP && level < maxlevel)
    {
        newXP -= nextLvlXP;
        ++level;

        GivePetLevel(level);                              // also update UNIT_FIELD_PETNEXTLEVELEXP and UNIT_FIELD_PETEXPERIENCE to level start

        nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    }

    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, level < maxlevel ? newXP : 0);

    if (getPetType() == HUNTER_PET)
    {
        KillLoyaltyBonus(level);
    }
}

/**
 * @brief Sets the pet to a new level and refreshes level-dependent stats.
 *
 * @param level The new level.
 */
void Pet::GivePetLevel(uint32 level)
{
    if (!level || level == getLevel())
    {
        return;
    }

    if (getPetType() == HUNTER_PET)
    {
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(level));
    }

    InitStatsForLevel(level);
    SetTP(m_TrainingPoints + (GetLoyaltyLevel() - 1));
}

/**
 * @brief Initializes base pet data from an existing creature.
 *
 * @param creature The source creature.
 * @return true if initialization succeeded; otherwise, false.
 */
bool Pet::CreateBaseAtCreature(Creature* creature)
{
    if (!creature)
    {
        sLog.outError("CRITICAL: NULL pointer passed into CreateBaseAtCreature()");
        return false;
    }

    CreatureCreatePos pos(creature, creature->GetOrientation());

    uint32 guid = creature->GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);

    uint32 pet_number = sObjectMgr.GeneratePetNumber();
    if (!Create(guid, pos, creature->GetCreatureInfo(), pet_number))
    {
        return false;
    }

    CreatureInfo const* cinfo = GetCreatureInfo();
    if (!cinfo)
    {
        sLog.outError("CreateBaseAtCreature() failed, creatureInfo is missing!");
        return false;
    }

    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        setPetType(MINI_PET);
        return true;
    }
    SetDisplayId(creature->GetDisplayId());
    SetNativeDisplayId(creature->GetNativeDisplayId());
    SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
    SetPower(POWER_HAPPINESS, 166500);
    SetPowerType(POWER_FOCUS);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(creature->getLevel()));
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cinfo->Family))
    {
        SetName(cFamily->Name[sWorld.GetDefaultDbcLocale()]);
    }
    else
    {
        SetName(creature->GetNameForLocaleIdx(sObjectMgr.GetDBCLocaleIndex()));
    }

    m_loyaltyPoints = 1000;
    if (cinfo->CreatureType == CREATURE_TYPE_BEAST)
    {
        SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
        SetByteValue(UNIT_FIELD_BYTES_0, 2, GENDER_NONE);
        SetByteValue(UNIT_FIELD_BYTES_0, 3, POWER_FOCUS);
        SetSheath(SHEATH_STATE_MELEE);
        SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_AURAS | UNIT_BYTE2_FLAG_UNK5);
        SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE | UNIT_FLAG_RESTING | UNIT_FLAG_RENAME);

        SetUInt32Value(UNIT_MOD_CAST_SPEED, creature->GetUInt32Value(UNIT_MOD_CAST_SPEED));
        SetLoyaltyLevel(REBELLIOUS);
    }
    return true;
}

/**
 * @brief Initializes pet stats for a given level.
 *
 * @param petlevel The target pet level.
 * @param owner Optional owner override.
 * @return true if initialization succeeded; otherwise, false.
 */
bool Pet::InitStatsForLevel(uint32 petlevel, Unit* owner)
{
    CreatureInfo const* cinfo = GetCreatureInfo();
    MANGOS_ASSERT(cinfo);

    if (!owner)
    {
        owner = GetOwner();
        if (!owner)
        {
            sLog.outError("attempt to summon pet (Entry %u) without owner! Attempt terminated.", cinfo->Entry);
            return false;
        }
    }

    uint32 creature_ID = cinfo->Entry;

    switch (getPetType())
    {
        case SUMMON_PET:
            SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_MAGE);

            // this enables popup window (pet dismiss, cancel)
            SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
            break;
        case HUNTER_PET:
            SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
            SetByteValue(UNIT_FIELD_BYTES_0, 2, GENDER_NONE);
            SetSheath(SHEATH_STATE_MELEE);
            SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_AURAS | UNIT_BYTE2_FLAG_UNK5);

            // this enables popup window (pet abandon, cancel), original value set in CreateBaseAtCreature
            SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE | UNIT_FLAG_RESTING);
            break;
        case GUARDIAN_PET:
        case MINI_PET:
        default:
            break;
    }

    SetLevel(petlevel);

    SetMeleeDamageSchool(SpellSchools(cinfo->DamageSchool));

    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(petlevel * 50));

    SetAttackTime(BASE_ATTACK, cinfo->MeleeBaseAttackTime);
    SetAttackTime(OFF_ATTACK, cinfo->MeleeBaseAttackTime);
    SetAttackTime(RANGED_ATTACK, cinfo->RangedBaseAttackTime);

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0);

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cinfo->Family);
    if (cFamily && cFamily->minScale > 0.0f && getPetType() == HUNTER_PET)
    {
        float scale;
        if (getLevel() >= cFamily->maxScaleLevel)
        {
            scale = cFamily->maxScale;
        }
        else if (getLevel() <= cFamily->minScaleLevel)
        {
            scale = cFamily->minScale;
        }
        else
        {
            scale = cFamily->minScale + float(getLevel() - cFamily->minScaleLevel) / cFamily->maxScaleLevel * (cFamily->maxScale - cFamily->minScale);
        }

        SetObjectScale(scale);
        UpdateModelData();
    }
    m_bonusdamage = 0;

    int32 createResistance[MAX_SPELL_SCHOOL] = {0, 0, 0, 0, 0, 0, 0};

    if (getPetType() != HUNTER_PET)
    {
        createResistance[SPELL_SCHOOL_HOLY]   = cinfo->ResistanceHoly;
        createResistance[SPELL_SCHOOL_FIRE]   = cinfo->ResistanceFire;
        createResistance[SPELL_SCHOOL_NATURE] = cinfo->ResistanceNature;
        createResistance[SPELL_SCHOOL_FROST]  = cinfo->ResistanceFrost;
        createResistance[SPELL_SCHOOL_SHADOW] = cinfo->ResistanceShadow;
        createResistance[SPELL_SCHOOL_ARCANE] = cinfo->ResistanceArcane;
    }

    switch (getPetType())
    {
        case SUMMON_PET:
        {
            if (owner->GetTypeId() == TYPEID_PLAYER)
            {
                switch (owner->getClass())
                {
                    case CLASS_WARLOCK:
                    {
                        // the damage bonus used for pets is either fire or shadow damage, whatever is higher
                        uint32 fire  = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE);
                        uint32 shadow = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW);
                        uint32 val  = (fire > shadow) ? fire : shadow;

                        SetBonusDamage(int32(val * 0.15f));
                        // bonusAP += val * 0.57;
                        break;
                    }
                    case CLASS_MAGE:
                    {
                        // 40% damage bonus of mage's frost damage
                        float val = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FROST) * 0.4f;
                        if (val < 0)
                        {
                            val = 0;
                        }
                        SetBonusDamage(int32(val));
                        break;
                    }
                    default:
                        break;
                }
            }

            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));

            // SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(cinfo->attackpower));

            PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(creature_ID, petlevel);
            if (pInfo)                                      // exist in DB
            {
                SetCreateHealth(pInfo->health);
                SetCreateMana(pInfo->mana);

                if (pInfo->armor > 0)
                {
                    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));
                }

                for (int stat = 0; stat < MAX_STATS; ++stat)
                {
                    SetCreateStat(Stats(stat), float(pInfo->stats[stat]));
                }
            }
            else                                            // not exist in DB, use some default fake data
            {
                sLog.outErrorDb("Summoned pet (Entry: %u) not have pet stats data in DB", cinfo->Entry);

                // remove elite bonuses included in DB values
                SetCreateHealth(uint32(((float(cinfo->MaxLevelHealth) / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));
                SetCreateMana(uint32(((float(cinfo->MaxLevelMana)   / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));

                SetCreateStat(STAT_STRENGTH, 22);
                SetCreateStat(STAT_AGILITY, 22);
                SetCreateStat(STAT_STAMINA, 25);
                SetCreateStat(STAT_INTELLECT, 28);
                SetCreateStat(STAT_SPIRIT, 27);
            }
            break;
        }
        case HUNTER_PET:
        {
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(petlevel));
            // these formula may not be correct; however, it is designed to be close to what it should be
            // this makes dps 0.5 of pets level
            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));
            // damage is modified afterwards based on creature attack power and attack speed

            // stored standard pet stats are entry 1 in pet_levelinfo
            PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(creature_ID, petlevel);
            if (pInfo)                                      // exist in DB
            {
                SetCreateHealth(pInfo->health);
                SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));
                // SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(cinfo->attackpower));

                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                {
                    SetCreateStat(Stats(i),  float(pInfo->stats[i]));
                }
            }
            else                                            // not exist in DB, use some default fake data
            {
                sLog.outErrorDb("Hunter pet levelstats missing in DB");

                // remove elite bonuses included in DB values
                SetCreateHealth(uint32(((float(cinfo->MaxLevelHealth) / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));

                SetCreateStat(STAT_STRENGTH, 22);
                SetCreateStat(STAT_AGILITY, 22);
                SetCreateStat(STAT_STAMINA, 25);
                SetCreateStat(STAT_INTELLECT, 28);
                SetCreateStat(STAT_SPIRIT, 27);
            }
            break;
        }
        case GUARDIAN_PET:
            SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);

            SetCreateMana(28 + 10 * petlevel);
            SetCreateHealth(28 + 30 * petlevel);

            // FIXME: this is wrong formula, possible each guardian pet have own damage formula
            // these formula may not be correct; however, it is designed to be close to what it should be
            // this makes dps 0.5 of pets level
            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
            // damage range is then petlevel / 2
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));
            break;
        default:
            sLog.outError("Pet have incorrect type (%u) for levelup.", getPetType());
            break;
    }

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, float(createResistance[i]));
    }

    UpdateAllStats();

    SetHealth(GetMaxHealth());
    SetPower(GetPowerType(), GetMaxPower(GetPowerType()));

    return true;
}

/**
 * @brief Checks whether the pet can eat a specific food item.
 *
 * @param item The food item prototype.
 * @return true if the food is valid for this pet; otherwise, false.
 */
bool Pet::HaveInDiet(ItemPrototype const* item) const
{
    if (!item->FoodType)
    {
        return false;
    }

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        return false;
    }

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
    {
        return false;
    }

    uint32 diet = cFamily->petFoodMask;
    uint32 FoodMask = 1 << (item->FoodType - 1);
    return diet & FoodMask;
}

/**
 * @brief Computes the happiness benefit gained from a food level.
 *
 * @param itemlevel The level of the consumed food item.
 * @return The happiness benefit value.
 */
uint32 Pet::GetCurrentFoodBenefitLevel(uint32 itemlevel)
{
    // -5 or greater food level
    if (getLevel() <= itemlevel + 5)                        // possible to feed level 60 pet with level 55 level food for full effect
    {
        return 35000;
    }
    // -10..-6
    else if (getLevel() <= itemlevel + 10)                  // pure guess, but sounds good
    {
        return 17000;
    }
    // -14..-11
    else if (getLevel() <= itemlevel + 14)                  // level 55 food gets green on 70, makes sense to me
    {
        return 8000;
    }
    // -15 or less
    else
    {
        return 0;                                           // food too low level
    }
}













/**
 * @brief Checks whether a teachable pet spell should teach the owner.
 *
 * @param spellid The pet spell that may trigger teaching.
 */
void Pet::CheckLearning(uint32 spellid)
{
    // charmed case -> prevent crash
    if (GetTypeId() == TYPEID_PLAYER || getPetType() != HUNTER_PET)
    {
        return;
    }

    Unit* owner = GetOwner();

    if (m_teachspells.empty() || !owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    TeachSpellMap::iterator itr = m_teachspells.find(spellid);
    if (itr == m_teachspells.end())
    {
        return;
    }

    if (urand(0, 100) < 10)
    {
        ((Player*)owner)->learnSpell(itr->second, false);
        m_teachspells.erase(itr);
    }
}



/**
 * @brief Checks whether the pet is permanent for a player.
 *
 * @param owner The owning player.
 * @return true if the pet should persist; otherwise, false.
 */
bool Pet::IsPermanentPetFor(Player* owner)
{
    switch (getPetType())
    {
        case SUMMON_PET:
            switch (owner->getClass())
            {
                // oddly enough, Mage's Water Elemental is still treated as temporary pet with Glyph of Eternal Water
                // i.e. does not unsummon at mounting, gets dismissed at teleport etc.
                case CLASS_WARLOCK:
                    return GetCreatureInfo()->CreatureType == CREATURE_TYPE_DEMON;
                default:
                    return false;
            }
        case HUNTER_PET:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Creates the pet world object from creature data.
 *
 * @param guidlow The low GUID to use.
 * @param cPos The creation position.
 * @param cinfo The creature template.
 * @param pet_number The pet number identifier.
 * @return true if creation succeeded; otherwise, false.
 */
bool Pet::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number)
{
    SetMap(cPos.GetMap());

    Object::_Create(guidlow, pet_number, HIGHGUID_PET);

    m_originalEntry = cinfo->Entry;

    if (!InitEntry(cinfo->Entry))
    {
        return false;
    }

    cPos.SelectFinalPoint(this);

    if (!cPos.Relocate(this))
    {
        return false;
    }

    SetSheath(SHEATH_STATE_MELEE);
    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_AURAS | UNIT_BYTE2_FLAG_UNK5);

    if (getPetType() == MINI_PET)                           // always non-attackable
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
    }

    return true;
}

/**
 * @brief Checks whether the pet currently knows a spell.
 *
 * @param spell The spell identifier.
 * @return true if the spell is present and not removed; otherwise, false.
 */
bool Pet::HasSpell(uint32 spell) const
{
    PetSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PETSPELL_REMOVED);
}

// Get all passive spells in our skill line

/**
 * @brief Learns passive family spells for the pet.
 */
void Pet::LearnPetPassives()
{
    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        return;
    }

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
    {
        return;
    }

    PetFamilySpellsStore::const_iterator petStore = sPetFamilySpellsStore.find(cFamily->ID);
    if (petStore != sPetFamilySpellsStore.end())
    {
        for (PetFamilySpellsSet::const_iterator petSet = petStore->second.begin(); petSet != petStore->second.end(); ++petSet)
        {
            addSpell(*petSet, ACT_DECIDE, PETSPELL_NEW, PETSPELL_FAMILY);
        }
    }
}

/**
 * @brief Applies owner pet auras to the pet.
 *
 * @param current true if this is the currently summoned permanent pet.
 */
void Pet::CastPetAuras(bool current)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!IsPermanentPetFor((Player*)owner))
    {
        return;
    }

    for (PetAuraSet::const_iterator itr = owner->m_petAuras.begin(); itr != owner->m_petAuras.end();)
    {
        PetAura const* pa = *itr;
        ++itr;

        if (!current && pa->IsRemovedOnChangePet())
        {
            owner->RemovePetAura(pa);
        }
        else
        {
            CastPetAura(pa);
        }
    }
}

/**
 * @brief Applies owner talent auras that should affect the pet.
 */
void Pet::CastOwnerTalentAuras()
{
    if (!GetOwner() || GetOwner()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    // Add below code handling spells cast by pet when owner/player has aura from talent
}

/**
 * @brief Casts a specific pet aura effect.
 *
 * @param aura The pet aura definition to apply.
 */
void Pet::CastPetAura(PetAura const* aura)
{
    uint32 auraId = aura->GetAura(GetEntry());
    if (!auraId)
    {
        return;
    }

    if (auraId == 35696)                                    // Demonic Knowledge
    {
        int32 basePoints = int32(aura->GetDamage() * (GetStat(STAT_STAMINA) + GetStat(STAT_INTELLECT)) / 100);
        CastCustomSpell(this, auraId, &basePoints, NULL, NULL, true);
    }
    else
    {
        CastSpell(this, auraId, true);
    }
}

/**
 * @brief Synchronizes pet level rules with the owner.
 */
void Pet::SynchronizeLevelWithOwner()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    switch (getPetType())
    {
        // always same level
        case SUMMON_PET:
            GivePetLevel(owner->getLevel());
            break;
        // can't be greater owner level
        case HUNTER_PET:
            if (getLevel() > owner->getLevel())
            {
                GivePetLevel(owner->getLevel());
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Applies or removes pet mode flags and updates the owner client.
 *
 * @param mode The mode flag to modify.
 * @param apply true to set the flag; false to clear it.
 */
void Pet::ApplyModeFlags(PetModeFlags mode, bool apply)
{
    if (apply)
    {
        m_petModeFlags = PetModeFlags(m_petModeFlags | mode);
    }
    else
    {
        m_petModeFlags = PetModeFlags(m_petModeFlags & ~mode);
    }

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    WorldPacket data(SMSG_PET_MODE, 12);
    data << GetObjectGuid();
    data << uint32(m_petModeFlags);
    owner->ToPlayer()->SendDirectMessage(&data);
}

/**
 * @brief Updates pet movement speed relative to owner modifiers.
 *
 * @param mtype The movement type to update.
 * @param forced true to force an update packet.
 * @param ratio Additional speed ratio multiplier.
 */
void Pet::UpdateSpeed(UnitMoveType mtype, bool forced, float ratio)
{
    Unit* unitOwner = GetOwner();
    Player *owner = unitOwner ? unitOwner->ToPlayer() : NULL;
    if (!owner)
    {
        return Unit::UpdateSpeed(mtype, forced, ratio);         // NPC pets are usual creatures
    }

    int32 main_speed_mod  = 0;
    float stack_bonus     = 1.0f;
    float non_stack_bonus = 1.0f;

    switch (mtype)
    {
        case MOVE_WALK:
            break;
        case MOVE_RUN:
            if (!m_attacking && owner->HasAura(19596))   // Bestial Swiftness: prevent while following
            {
                AuraList const& auras = GetAurasByType(SPELL_AURA_MOD_INCREASE_SPEED);
                for (AuraList::const_iterator it = auras.begin(); it != auras.end(); ++it)
                {
                    if ((*it)->GetId() != 19582)                        // exclude the aura influenced by Bestial Swiftness
                    {
                        main_speed_mod = std::max((*it)->GetBasePoints(), main_speed_mod);
                    }
                }
            }
            else
            {
                main_speed_mod = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED);
            }

            stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
            non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_SPEED_NOT_STACK)) / 100.0f;
            break;
        case MOVE_RUN_BACK:
            return;
        case MOVE_SWIM:
        {
            main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED);
            break;
        }
        case MOVE_SWIM_BACK:
            return;
        default:
            sLog.outError("Pet::UpdateSpeed: Unsupported move type (%d)", mtype);
            return;
    }

    // Get owner current speed
    float ownerSpeed = owner->GetSpeedRate(mtype);
    int32 slow = owner->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);

    // If owner is affected by speed reduction effects, do not take them into account
    // (a dazed hunter does not affect pet's speed)
    if (slow)
    {
        ownerSpeed *= 100.0f / (100.0f + slow) ;
    }

    float speed = std::max(non_stack_bonus, stack_bonus) * ownerSpeed;

    if (main_speed_mod)
    {
        speed = speed * (100.0f + main_speed_mod) / 100.0f;
    }

    switch (mtype)
    {
        case MOVE_RUN:
        case MOVE_SWIM:
        {
            // Normalize speed by 191 aura SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED if need
            // TODO: possible affect only on MOVE_RUN
            if (int32 normalization = GetMaxPositiveAuraModifier(SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED))
            {
                // Use speed from aura
                float max_speed = normalization / baseMoveSpeed[mtype];
                if (speed > max_speed)
                {
                    speed = max_speed;
                }
            }
            break;
        }
        default:
            break;
    }

    // Apply strongest slow aura mod to speed
    slow = GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);
    if (slow)
    {
        speed *= (100.0f + slow) / 100.0f;
    }

    if (mtype == MOVE_RUN)
    {
        speed *= 1.14286f;
    }

    SetSpeedRate(mtype, speed * ratio, forced);
}

/**
 * @brief Checks database state for an owner's current or available pet.
 *
 * @param owner The player whose pet data is being checked.
 * @return The detected pet database status.
 */
PetDatabaseStatus Pet::GetStatusFromDB(Player* owner)
{
    PetDatabaseStatus status = PET_DB_NO_PET;

    uint32 ownerid = owner->GetGUIDLow();

    QueryResult* result;
    //                                         0     1        2        3          4        5      6             7                8          9             10      11      12         13           14         15              16        17                18          19                   20                   21                22
    result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
        "FROM `character_pet` WHERE `owner` = %u AND (`slot` = %u OR `slot` > %u)",
        ownerid, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);
    if (!result)
    {
        return status;
    }

    Field* fields = result->Fetch();

    uint32 petentry = fields[1].GetUInt32();

    if (!petentry)
    {
        delete result;
        return status;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        delete result;
        return status;
    }

    uint32 savedHP = fields[13].GetUInt32();
    delete result;
    if (savedHP > 0)
    {
        status = PET_DB_ALIVE;
    }
    else
    {
        status = PET_DB_DEAD;
    }

    return status;
}
