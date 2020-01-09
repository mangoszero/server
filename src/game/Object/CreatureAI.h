/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_CREATUREAI_H
#define MANGOS_CREATUREAI_H

#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

class WorldObject;
class GameObject;
class Unit;
class Creature;
class Player;
struct SpellEntry;
class ChatHandler;

#define TIME_INTERVAL_LOOK   5000
#define VISIBILITY_RANGE    10000

enum CanCastResult
{
    CAST_OK                     = 0,
    CAST_FAIL_IS_CASTING        = 1,
    CAST_FAIL_OTHER             = 2,
    CAST_FAIL_TOO_FAR           = 3,
    CAST_FAIL_TOO_CLOSE         = 4,
    CAST_FAIL_POWER             = 5,
    CAST_FAIL_STATE             = 6,
    CAST_FAIL_TARGET_AURA       = 7,
    CAST_FAIL_NO_LOS            = 8,
    CAST_FAIL_SILENCED          = 9
};

enum CastFlags
{
    CAST_INTERRUPT_PREVIOUS     = 0x01,                     // Interrupt any spell casting
    CAST_TRIGGERED              = 0x02,                     // Triggered (this makes spell cost zero mana and have no cast time)
    CAST_FORCE_CAST             = 0x04,                     // Forces cast even if creature is out of mana or out of range
    CAST_NO_MELEE_IF_OOM        = 0x08,                     // Prevents creature from entering melee if out of mana or out of range
    CAST_FORCE_TARGET_SELF      = 0x10,                     // Forces the target to cast this spell on itself
    CAST_AURA_NOT_PRESENT       = 0x20,                     // Only casts the spell if the target does not have an aura from the spell
};

enum CombatMovementFlags
{
    CM_SCRIPT = 0x01,
    CM_SPELL  = 0x02,
};

enum AIEventType
{
    // Usable with Event AI
    AI_EVENT_JUST_DIED          = 0,                        // Sender = Killed Npc, Invoker = Killer
    AI_EVENT_CRITICAL_HEALTH    = 1,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 10% health
    AI_EVENT_LOST_HEALTH        = 2,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 50% health
    AI_EVENT_LOST_SOME_HEALTH   = 3,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 90% health
    AI_EVENT_GOT_FULL_HEALTH    = 4,                        // Sender = Healed Npc, Invoker = Healer
    AI_EVENT_CUSTOM_EVENTAI_A   = 5,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists)
    AI_EVENT_CUSTOM_EVENTAI_B   = 6,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists)
    AI_EVENT_GOT_CCED           = 7,                        // Sender = CCed Npc, Invoker = Caster that CCed
    MAXIMAL_AI_EVENT_EVENTAI    = 8,

    // Internal Use
    AI_EVENT_CALL_ASSISTANCE    = 10,                       // Sender = Attacked Npc, Invoker = Enemy

    // Predefined for SD3
    AI_EVENT_START_ESCORT       = 100,                      // Invoker = Escorting Player
    AI_EVENT_START_ESCORT_B     = 101,                      // Invoker = Escorting Player
    AI_EVENT_START_EVENT        = 102,                      // Invoker = EventStarter
    AI_EVENT_START_EVENT_A      = 103,                      // Invoker = EventStarter
    AI_EVENT_START_EVENT_B      = 104,                      // Invoker = EventStarter

    // Some IDs for special cases in SD3
    AI_EVENT_CUSTOM_A           = 1000,
    AI_EVENT_CUSTOM_B           = 1001,
    AI_EVENT_CUSTOM_C           = 1002,
    AI_EVENT_CUSTOM_D           = 1003,
    AI_EVENT_CUSTOM_E           = 1004,
    AI_EVENT_CUSTOM_F           = 1005,
};

class CreatureAI
{
    public:
        explicit CreatureAI(Creature* creature);
        virtual ~CreatureAI();

        ///== Information about AI ========================
        /**
         * This function is used to display information about the AI.
         * It is called when the .npc aiinfo command is used.
         * Use this for on-the-fly debugging
         * @param reader is a ChatHandler to send messages to.
         */
        virtual void GetAIInformation(ChatHandler& /*reader*/) {}

        ///== Reactions At =================================

        /**
         * Called if IsVisible(Unit* pWho) is true at each (relative) override pWho move, reaction at visibility zone enter
         * Note: The Unit* pWho can be out of Line of Sight, usually this is only visibiliy (by state) and range dependendend
         * Note: This function is not called for creatures who are in evade mode
         * @param pWho Unit* who moved in the visibility range and is visisble
         */
        virtual void MoveInLineOfSight(Unit* /*pWho*/) {}

        /**
         * Called for reaction at enter to combat if not in combat yet
         * @param pEnemy Unit* of whom the Creature enters combat with, can be NULL
         */
        virtual void EnterCombat(Unit* /*pEnemy*/) {}

        /**
         * Called for reaction at stopping attack at no attackers or targets
         * This is called usually in Unit::SelectHostileTarget, if no more target exists
         */
        virtual void EnterEvadeMode();

        /**
         * Called at reaching home after MoveTargetedHome
         */
        virtual void JustReachedHome() {}

        /**
         * Called at any Heal received from any Unit
         * @param pHealer Unit* which deals the heal
         * @param uiHealedAmount Amount of healing received
         */
        virtual void HealedBy(Unit * /*pHealer*/, uint32& /*uiHealedAmount*/) {}

        /**
         * Called at any Damage to any victim (before damage apply)
         * @param pDoneTo Unit* to whom Damage of amount uiDamage will be dealt
         * @param uiDamage Amount of Damage that will be dealt, can be changed here
         */
        virtual void DamageDeal(Unit* /*pDoneTo*/, uint32& /*uiDamage*/) {}

        /**
         * Called at any Damage from any attacker (before damage apply)
         * Note:    Use for recalculation damage or special reaction at damage
         *          for attack reaction use AttackedBy called for not DOT damage in Unit::DealDamage also
         * @param pDealer Unit* who will deal Damage to the creature
         * @param uiDamage Amount of Damage that will be dealt, can be changed here
         */
        virtual void DamageTaken(Unit* /*pDealer*/, uint32& /*uiDamage*/) {}

        /**
         * Called when the creature is killed
         * @param pKiller Unit* who killed the creature
         */
        virtual void JustDied(Unit* /*pKiller*/) {}

        /**
         * Called when the corpse of this creature gets removed
         * @param uiRespawnDelay Delay (in seconds). If != 0, then this is the time after which the creature will respawn, if = 0 the default respawn-delay will be used
         */
        virtual void CorpseRemoved(uint32& /*uiRespawnDelay*/) {}

        /**
         * Called when a summoned creature is killed
         * @param pSummoned Summoned Creature* that got killed
         */
        virtual void SummonedCreatureJustDied(Creature* /*pSummoned*/) {}

        /**
         * Called when the creature kills a unit
         * @param pVictim Victim that got killed
         */
        virtual void KilledUnit(Unit* /*pVictim*/) {}

        /**
         * Called when owner of m_creature (if m_creature is PROTECTOR_PET) kills a unit
         * @param pVictim Victim that got killed (by owner of creature)
         */
        virtual void OwnerKilledUnit(Unit* /*pVictim*/) {}

        /**
         * Called when the creature summon successfully other creature
         * @param pSummoned Creature that got summoned
         */
        virtual void JustSummoned(Creature* /*pSummoned*/) {}

        /**
         * Called when the creature summon successfully a gameobject
         * @param pGo GameObject that was summoned
         */
        virtual void JustSummoned(GameObject* /*pGo*/) {}

        /**
         * Called when a summoned creature gets TemporarySummon::UnSummon ed
         * @param pSummoned Summoned creature that despawned
         */
        virtual void SummonedCreatureDespawn(Creature* /*pSummoned*/) {}

        /**
         * Called when hit by a spell
         * @param pCaster Caster who casted the spell
         * @param pSpell The spell that hit the creature
         */
        virtual void SpellHit(Unit* /*pCaster*/, const SpellEntry* /*pSpell*/) {}

        /**
         * Called when the current casted spell is processed
         * @param pSpell The spell that is casted currently
         * @param reason The spell state (see SpellCastResult enum)
         */
        virtual void OnSpellCastChange(const SpellEntry* /*pSpell*/, SpellCastResult /*reason*/) {}
        /**
         * Called when spell hits creature's target
         * @param pTarget Target that we hit with the spell
         * @param pSpell Spell with which we hit pTarget
         */
        virtual void SpellHitTarget(Unit* /*pTarget*/, const SpellEntry* /*pSpell*/) {}

        /**
         * Called when the creature is target of hostile action: swing, hostile spell landed, fear/etc)
         * @param pAttacker Unit* who attacked the creature
         */
        virtual void AttackedBy(Unit* pAttacker);

        /**
         * Called when creature is respawned (for reseting variables)
         */
        virtual void JustRespawned() {}

        /**
         * Called at waypoint reached or point movement finished
         * @param uiMovementType Type of the movement (enum MovementGeneratorType)
         * @param uiData Data related to the finished movement (ie point-id)
         */
        virtual void MovementInform(uint32 /*uiMovementType*/, uint32 /*uiData*/) {}

        /**
         * Called if a temporary summoned of m_creature reach a move point
         * @param pSummoned Summoned Creature that finished some movement
         * @param uiMotionType Type of the movement (enum MovementGeneratorType)
         * @param uiData Data related to the finished movement (ie point-id)
         */
        virtual void SummonedMovementInform(Creature* /*pSummoned*/, uint32 /*uiMotionType*/, uint32 /*uiData*/) {}

        /**
         * Called at text emote receive from player
         * @param pPlayer Player* who sent the emote
         * @param uiEmote ID of the emote the player used with the creature as target
         */
        virtual void ReceiveEmote(Player* /*pPlayer*/, uint32 /*uiEmote*/) {}

        ///== Triggered Actions Requested ==================

        /**
         * Called when creature attack expected (if creature can and no have current victim)
         * Note: for reaction at hostile action must be called AttackedBy function.
         * Note: Usually called by MoveInLineOfSight, in Unit::SelectHostileTarget or when the AI is forced to attack an enemy
         * @param pWho Unit* who is possible target
         */
        virtual void AttackStart(Unit* /*pWho*/) {}

        /**
         * Called at World update tick, by default every 100ms
         * This setting is dependend on CONFIG_UINT32_INTERVAL_MAPUPDATE
         * Note: Use this function to handle Timers, Threat-Management and MeleeAttacking
         * @param uiDiff Passed time since last call
         */
        virtual void UpdateAI(const uint32 /*uiDiff*/) {}

        ///== State checks =================================

        /**
         * Check if unit is visible for MoveInLineOfSight
         * Note: This check is by default only the state-depending (visibility, range), NOT LineOfSight
         * @param pWho Unit* who is checked if it is visisble for the creature
         */
        virtual bool IsVisible(Unit* /*pWho*/) const { return false; }

        // Called when victim entered water and creature can not enter water
        // TODO: rather unused
        virtual bool canReachByRangeAttack(Unit*) { return false; }

        ///== Helper functions =============================

        /// This function is used to do the actual melee damage (if possible)
        bool DoMeleeAttackIfReady();

        /// Internal helper function, to check if a spell can be cast
        CanCastResult CanCastSpell(Unit* pTarget, const SpellEntry* pSpell, bool isTriggered);

        /**
         * Function to cast a spell if possible
         * @param pTarget Unit* onto whom the spell should be cast
         * @param uiSpell ID of the spell that the creature will try to cast
         * @param uiCastFlags Some flags to define how to cast, see enum CastFlags
         * @param OriginalCasterGuid the original caster of the spell if required, empty by default
         */
        virtual CanCastResult DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags = 0, ObjectGuid OriginalCasterGuid = ObjectGuid());

        /// Combat movement functions
        void SetCombatMovement(bool enable, bool stopOrStartMovement = false);
        bool IsCombatMovement() const { return m_combatMovement != 0; }
        uint8 GetCombatMovementFlags() const { return m_combatMovement; }
        void SetCombatMovementFlag(uint8 flag, bool setFlag = true);

        ///== Event Handling ===============================

        /*
         * Send an AI Event to nearby Creatures around
         * @param uiType number to specify the event, default cases listed in enum AIEventType
         * @param pInvoker Unit that triggered this event (like an attacker)
         * @param uiDelay  delay time until the Event will be triggered
         * @param fRadius  range in which for receiver is searched
         */
        void SendAIEventAround(AIEventType eventType, Unit* pInvoker, uint32 uiDelay, float fRadius, uint32 miscValue = 0) const;

        /*
         * Send an AI Event to a Creature
         * @param eventType to specify the event, default cases listed in enum AIEventType
         * @param pInvoker Unit that triggered this event (like an attacker)
         * @param pReceiver Creature to receive this event
         */
        void SendAIEvent(AIEventType eventType, Unit* pInvoker, Creature* pReceiver, uint32 miscValue = 0) const;

        /*
         * Called when an AI Event is received
         * @param eventType to specify the event, default cases listed in enum AIEventType
         * @param pSender Creature that sent this event
         * @param pInvoker Unit that triggered this event (like an attacker)
         */
        virtual void ReceiveAIEvent(AIEventType /*eventType*/, Creature* /*pSender*/, Unit* /*pInvoker*/, uint32 /*miscValue*/) {}

        // Reset should be defined here, as it is called from out the AI ctor now
        virtual void Reset() { m_combatMovement = CM_SCRIPT; }

    protected:
        void HandleMovementOnAttackStart(Unit* victim);
        void SetChase(bool chase);

        ///== Fields =======================================

        /// Pointer to the Creature controlled by this AI
        Creature* const m_creature;

        /// How should an enemy be chased
        float m_attackDistance;
        float m_attackAngle;
    private:
        /// Combat movement currently enabled
        uint8 m_combatMovement;
};

struct SelectableAI : public FactoryHolder<CreatureAI>, public Permissible<Creature>
{
    SelectableAI(const char* id) : FactoryHolder<CreatureAI>(id) {}
};

template<class REAL_AI>
struct CreatureAIFactory : public SelectableAI
{
    CreatureAIFactory(const char* name) : SelectableAI(name) {}

    CreatureAI* Create(void*) const override;

    virtual int Permit(const Creature* c) const override { return REAL_AI::Permissible(c); }
};

enum Permitions
{
    PERMIT_BASE_NO                 = -1,
    PERMIT_BASE_IDLE               = 1,
    PERMIT_BASE_REACTIVE           = 100,
    PERMIT_BASE_PROACTIVE          = 200,
    PERMIT_BASE_FACTION_SPECIFIC   = 400,
    PERMIT_BASE_SPECIAL            = 800
};

typedef FactoryHolder<CreatureAI> CreatureAICreator;
typedef FactoryHolder<CreatureAI>::FactoryHolderRegistry CreatureAIRegistry;
typedef FactoryHolder<CreatureAI>::FactoryHolderRepository CreatureAIRepository;

#endif
