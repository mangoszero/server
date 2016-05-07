/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2016  MaNGOS project <https://getmangos.eu>
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

/**
 * \addtogroup game
 * @{
 * \file
 */
#ifndef MANGOS_H_UNIT
#define MANGOS_H_UNIT

#include "Common.h"
#include "Object.h"
#include "Opcodes.h"
#include "SpellAuraDefines.h"
#include "UpdateFields.h"
#include "SharedDefines.h"
#include "ThreatManager.h"
#include "HostileRefManager.h"
#include "FollowerReference.h"
#include "FollowerRefManager.h"
#include "Utilities/EventProcessor.h"
#include "MotionMaster.h"
#include "DBCStructure.h"
#include "Path.h"
#include "WorldPacket.h"
#include "Timer.h"
#include "Log.h"
#include <list>

enum SpellInterruptFlags
{
    SPELL_INTERRUPT_FLAG_MOVEMENT     = 0x01,
    SPELL_INTERRUPT_FLAG_DAMAGE       = 0x02,
    SPELL_INTERRUPT_FLAG_INTERRUPT    = 0x04,
    SPELL_INTERRUPT_FLAG_AUTOATTACK   = 0x08,
    SPELL_INTERRUPT_FLAG_ABORT_ON_DMG = 0x10               // _complete_ interrupt on direct damage
    // SPELL_INTERRUPT_UNK               = 0x20               // unk, 564 of 727 spells having this spell start with "Glyph"
};

enum SpellChannelInterruptFlags
{
    CHANNEL_FLAG_DAMAGE      = 0x0002,
    CHANNEL_FLAG_MOVEMENT    = 0x0008,
    CHANNEL_FLAG_TURNING     = 0x0010,
    CHANNEL_FLAG_DAMAGE2     = 0x0080,
    CHANNEL_FLAG_DELAY       = 0x4000
};

enum SpellAuraInterruptFlags
{
    AURA_INTERRUPT_FLAG_UNK0                        = 0x00000001,   // 0    removed when getting hit by a negative spell?
    AURA_INTERRUPT_FLAG_DAMAGE                      = 0x00000002,   // 1    removed by any damage
    AURA_INTERRUPT_FLAG_UNK2                        = 0x00000004,   // 2
    AURA_INTERRUPT_FLAG_MOVE                        = 0x00000008,   // 3    removed by any movement
    AURA_INTERRUPT_FLAG_TURNING                     = 0x00000010,   // 4    removed by any turning
    AURA_INTERRUPT_FLAG_ENTER_COMBAT                = 0x00000020,   // 5    removed by entering combat
    AURA_INTERRUPT_FLAG_NOT_MOUNTED                 = 0x00000040,   // 6    removed by unmounting
    AURA_INTERRUPT_FLAG_NOT_ABOVEWATER              = 0x00000080,   // 7    removed by entering water
    AURA_INTERRUPT_FLAG_NOT_UNDERWATER              = 0x00000100,   // 8    removed by leaving water
    AURA_INTERRUPT_FLAG_NOT_SHEATHED                = 0x00000200,   // 9    removed by unsheathing
    AURA_INTERRUPT_FLAG_UNK10                       = 0x00000400,   // 10
    AURA_INTERRUPT_FLAG_UNK11                       = 0x00000800,   // 11
    AURA_INTERRUPT_FLAG_UNK12                       = 0x00001000,   // 12   removed by attack?
    AURA_INTERRUPT_FLAG_UNK13                       = 0x00002000,   // 13
    AURA_INTERRUPT_FLAG_UNK14                       = 0x00004000,   // 14
    AURA_INTERRUPT_FLAG_UNK15                       = 0x00008000,   // 15   removed by casting a spell?
    AURA_INTERRUPT_FLAG_UNK16                       = 0x00010000,   // 16
    AURA_INTERRUPT_FLAG_MOUNTING                    = 0x00020000,   // 17   removed by mounting
    AURA_INTERRUPT_FLAG_NOT_SEATED                  = 0x00040000,   // 18   removed by standing up (used by food and drink mostly and sleep/Fake Death like)
    AURA_INTERRUPT_FLAG_CHANGE_MAP                  = 0x00080000,   // 19   leaving map/getting teleported
    AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION    = 0x00100000,   // 20   removed by auras that make you invulnerable, or make other to loose selection on you
    AURA_INTERRUPT_FLAG_UNK21                       = 0x00200000,   // 21
    AURA_INTERRUPT_FLAG_UNK22                       = 0x00400000,   // 22
    AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT            = 0x00800000,   // 23   removed by entering pvp combat
    AURA_INTERRUPT_FLAG_DIRECT_DAMAGE               = 0x01000000    // 24   removed by any direct damage
};

enum SpellModOp
{
    SPELLMOD_DAMAGE                 = 0,
    SPELLMOD_DURATION               = 1,
    SPELLMOD_THREAT                 = 2,
    SPELLMOD_ATTACK_POWER           = 3,
    SPELLMOD_CHARGES                = 4,
    SPELLMOD_RANGE                  = 5,
    SPELLMOD_RADIUS                 = 6,
    SPELLMOD_CRITICAL_CHANCE        = 7,
    SPELLMOD_ALL_EFFECTS            = 8,
    SPELLMOD_NOT_LOSE_CASTING_TIME  = 9,
    SPELLMOD_CASTING_TIME           = 10,
    SPELLMOD_COOLDOWN               = 11,
    SPELLMOD_SPEED                  = 12,
    // spellmod 13 unused
    SPELLMOD_COST                   = 14,
    SPELLMOD_CRIT_DAMAGE_BONUS      = 15,
    SPELLMOD_RESIST_MISS_CHANCE     = 16,
    SPELLMOD_JUMP_TARGETS           = 17,
    SPELLMOD_CHANCE_OF_SUCCESS      = 18,                   // Only used with SPELL_AURA_ADD_FLAT_MODIFIER and affects proc spells
    SPELLMOD_ACTIVATION_TIME        = 19,
    SPELLMOD_EFFECT_PAST_FIRST      = 20,
    SPELLMOD_CASTING_TIME_OLD       = 21,
    SPELLMOD_DOT                    = 22,
    SPELLMOD_HASTE                  = 23,
    SPELLMOD_SPELL_BONUS_DAMAGE     = 24,
    // spellmod 25 unused
    // SPELLMOD_FREQUENCY_OF_SUCCESS   = 26,                
    SPELLMOD_MULTIPLE_VALUE         = 27,
    SPELLMOD_RESIST_DISPEL_CHANCE   = 28
};

#define MAX_SPELLMOD 32

enum SpellFacingFlags
{
    SPELL_FACING_FLAG_INFRONT = 0x0001
};

#define BASE_MELEERANGE_OFFSET 1.33f
#define BASE_MINDAMAGE 1.0f
#define BASE_MAXDAMAGE 2.0f
#define BASE_ATTACK_TIME 2000

/**
 * byte value (UNIT_FIELD_BYTES_1,0).
 *
 * This is not to be used as a bitmask but as one value
 * each, ie: you can't be standing and sitting down at
 * the same time.
 * \see Unit::getStandState
 * \see Unit::SetStandState
 * \see Unit::IsSitState
 * \see Unit::IsStandState
 */
enum UnitStandStateType
{
    UNIT_STAND_STATE_STAND             = 0,
    UNIT_STAND_STATE_SIT               = 1,
    UNIT_STAND_STATE_SIT_CHAIR         = 2,
    UNIT_STAND_STATE_SLEEP             = 3,
    UNIT_STAND_STATE_SIT_LOW_CHAIR     = 4,
    UNIT_STAND_STATE_SIT_MEDIUM_CHAIR  = 5,
    UNIT_STAND_STATE_SIT_HIGH_CHAIR    = 6,
    UNIT_STAND_STATE_DEAD              = 7,
    UNIT_STAND_STATE_KNEEL             = 8
};

#define MAX_UNIT_STAND_STATE             9

/* byte flag value not exist in 1.12, moved/merged in (UNIT_FIELD_BYTES_1,3), in post-1.x it's in (UNIT_FIELD_BYTES_1,2)
enum UnitStandFlags
*/

// byte flags value (UNIT_FIELD_BYTES_1,2)
// This corresponds to free talent points (pet case)

// byte flags value (UNIT_FIELD_BYTES_1,3)
enum UnitBytes1_Flags
{
    UNIT_BYTE1_FLAG_ALWAYS_STAND = 0x01,
    UNIT_BYTE1_FLAGS_CREEP       = 0x02,
    UNIT_BYTE1_FLAG_UNTRACKABLE  = 0x04,
    UNIT_BYTE1_FLAG_ALL          = 0xFF
};

/**
 *  byte value (UNIT_FIELD_BYTES_2,0)
 */
enum SheathState
{
    /// non prepared weapon
    SHEATH_STATE_UNARMED  = 0,
    /// prepared melee weapon
    SHEATH_STATE_MELEE    = 1,
    /// prepared ranged weapon
    SHEATH_STATE_RANGED   = 2
};

#define MAX_SHEATH_STATE    3

// byte flags value (UNIT_FIELD_BYTES_2,1)
enum UnitBytes2_Flags
{
    UNIT_BYTE2_FLAG_UNK0        = 0x01,
    UNIT_BYTE2_FLAG_UNK1        = 0x02,
    UNIT_BYTE2_FLAG_UNK2        = 0x04,
    UNIT_BYTE2_FLAG_UNK3        = 0x08,
    UNIT_BYTE2_FLAG_AURAS       = 0x10,                     // show positive auras as positive, and allow its dispel
    UNIT_BYTE2_FLAG_UNK5        = 0x20,
    UNIT_BYTE2_FLAG_UNK6        = 0x40,
    UNIT_BYTE2_FLAG_UNK7        = 0x80
};

#define CREATURE_MAX_SPELLS     4

enum Swing
{
    NOSWING                    = 0,
    SINGLEHANDEDSWING          = 1,
    TWOHANDEDSWING             = 2
};

/**
 * This seems to be the state a target of an attack can be in.
 * \todo Document more
 */
enum VictimState
{
    VICTIMSTATE_UNAFFECTED     = 0,                         // seen in relation with HITINFO_MISS
    VICTIMSTATE_NORMAL         = 1,
    VICTIMSTATE_DODGE          = 2,
    VICTIMSTATE_PARRY          = 3,
    VICTIMSTATE_INTERRUPT      = 4,
    VICTIMSTATE_BLOCKS         = 5,
    VICTIMSTATE_EVADES         = 6,
    VICTIMSTATE_IS_IMMUNE      = 7,
    VICTIMSTATE_DEFLECTS       = 8
};

/**
 * OFFSWING and BASESWING/2 or MAINSWING/2 to be more
 * in line with what is used in the other parts?
 *
 * \todo Rename the LEFTSWING and NORMALSWING/2 to:
 */
enum HitInfo
{
    HITINFO_NORMALSWING         = 0x00000000,
    HITINFO_UNK0                = 0x00000001,               // req correct packet structure
    HITINFO_NORMALSWING2        = 0x00000002,
    HITINFO_LEFTSWING           = 0x00000004,
    HITINFO_UNK3                = 0x00000008,
    HITINFO_MISS                = 0x00000010,
    HITINFO_ABSORB              = 0x00000020,               // plays absorb sound
    HITINFO_RESIST              = 0x00000040,               // resisted atleast some damage
    HITINFO_CRITICALHIT         = 0x00000080,
    HITINFO_UNK8                = 0x00000100,               // wotlk?
    HITINFO_UNK9                = 0x00002000,               // wotlk?
    HITINFO_GLANCING            = 0x00004000,
    HITINFO_CRUSHING            = 0x00008000,
    HITINFO_NOACTION            = 0x00010000,
    HITINFO_SWINGNOHITSOUND     = 0x00080000
};

// i would like to remove this: (it is defined in item.h
enum InventorySlot
{
    NULL_BAG                   = 0,
    NULL_SLOT                  = 255
};

struct FactionTemplateEntry;
struct Modifier;
struct SpellEntry;
struct SpellEntryExt;

class Aura;
class SpellAuraHolder;
class Creature;
class Spell;
class DynamicObject;
class GameObject;
class Item;
class Pet;
class PetAura;
class Totem;

struct SpellImmune
{
    uint32 type;
    uint32 spellId;
};

typedef std::list<SpellImmune> SpellImmuneList;

enum UnitModifierType
{
    BASE_VALUE = 0,
    BASE_PCT = 1,
    TOTAL_VALUE = 2,
    TOTAL_PCT = 3,
    MODIFIER_TYPE_END = 4
};

enum WeaponDamageRange
{
    MINDAMAGE,
    MAXDAMAGE
};

enum DamageTypeToSchool
{
    RESISTANCE,
    DAMAGE_DEALT,
    DAMAGE_TAKEN
};

/**
 * This is what decides how an \ref Aura was removed, the cause of it being removed.
 */
enum AuraRemoveMode
{
    AURA_REMOVE_BY_DEFAULT,
    AURA_REMOVE_BY_STACK,           ///< at replace by similar aura
    AURA_REMOVE_BY_CANCEL,          ///< It was cancelled by the user (needs confirmation)
    AURA_REMOVE_BY_DISPEL,          ///< It was dispelled by ie Remove Magic
    AURA_REMOVE_BY_DEATH,           ///< The \ref Unit died and there for it was removed 
    AURA_REMOVE_BY_DELETE,          ///< use for speedup and prevent unexpected effects at player logout/pet unsummon (must be used _only_ after save), delete.
    AURA_REMOVE_BY_SHIELD_BREAK,    ///< when absorb shield is removed by damage
    AURA_REMOVE_BY_EXPIRE,          ///< at duration end
    AURA_REMOVE_BY_TRACKING         ///< aura is removed because of a conflicting tracked aura
};

enum UnitMods
{
    UNIT_MOD_STAT_STRENGTH,                                 // UNIT_MOD_STAT_STRENGTH..UNIT_MOD_STAT_SPIRIT must be in existing order, it's accessed by index values of Stats enum.
    UNIT_MOD_STAT_AGILITY,
    UNIT_MOD_STAT_STAMINA,
    UNIT_MOD_STAT_INTELLECT,
    UNIT_MOD_STAT_SPIRIT,
    UNIT_MOD_HEALTH,
    UNIT_MOD_MANA,                                          // UNIT_MOD_MANA..UNIT_MOD_HAPPINESS must be in existing order, it's accessed by index values of Powers enum.
    UNIT_MOD_RAGE,
    UNIT_MOD_FOCUS,
    UNIT_MOD_ENERGY,
    UNIT_MOD_HAPPINESS,
    UNIT_MOD_ARMOR,                                         // UNIT_MOD_ARMOR..UNIT_MOD_RESISTANCE_ARCANE must be in existing order, it's accessed by index values of SpellSchools enum.
    UNIT_MOD_RESISTANCE_HOLY,
    UNIT_MOD_RESISTANCE_FIRE,
    UNIT_MOD_RESISTANCE_NATURE,
    UNIT_MOD_RESISTANCE_FROST,
    UNIT_MOD_RESISTANCE_SHADOW,
    UNIT_MOD_RESISTANCE_ARCANE,
    UNIT_MOD_ATTACK_POWER,
    UNIT_MOD_ATTACK_POWER_RANGED,
    UNIT_MOD_DAMAGE_MAINHAND,
    UNIT_MOD_DAMAGE_OFFHAND,
    UNIT_MOD_DAMAGE_RANGED,
    UNIT_MOD_END,
    // synonyms
    UNIT_MOD_STAT_START = UNIT_MOD_STAT_STRENGTH,
    UNIT_MOD_STAT_END = UNIT_MOD_STAT_SPIRIT + 1,
    UNIT_MOD_RESISTANCE_START = UNIT_MOD_ARMOR,
    UNIT_MOD_RESISTANCE_END = UNIT_MOD_RESISTANCE_ARCANE + 1,
    UNIT_MOD_POWER_START = UNIT_MOD_MANA,
    UNIT_MOD_POWER_END = UNIT_MOD_HAPPINESS + 1
};

enum BaseModGroup
{
    CRIT_PERCENTAGE,
    RANGED_CRIT_PERCENTAGE,
    OFFHAND_CRIT_PERCENTAGE,
    SHIELD_BLOCK_VALUE,
    BASEMOD_END
};

enum BaseModType
{
    FLAT_MOD,
    PCT_MOD,
    MOD_END,
};

enum DeathState
{
    ALIVE          = 0,     ///< show as alive
    JUST_DIED      = 1,     ///< temporary state at die, for creature auto converted to CORPSE, for player at next update call
    CORPSE         = 2,     ///< corpse state, for player this also meaning that player not leave corpse
    DEAD           = 3,     ///< for creature despawned state (corpse despawned), for player CORPSE/DEAD not clear way switches (FIXME), and use m_deathtimer > 0 check for real corpse state
    JUST_ALIVED    = 4      ///< temporary state at resurrection, for creature auto converted to ALIVE, for player at next update call
};

/**
 * internal state flags for some auras and movement generators, other. (Taken from comment)
 */
enum UnitState
{
    // persistent state (applied by aura/etc until expire)
    UNIT_STAT_MELEE_ATTACKING = 0x00000001,                 // unit is melee attacking someone Unit::Attack
    UNIT_STAT_ATTACK_PLAYER   = 0x00000002,                 // unit attack player or player's controlled unit and have contested pvpv timer setup, until timer expire, combat end and etc
    UNIT_STAT_DIED            = 0x00000004,                 // Unit::SetFeignDeath
    UNIT_STAT_STUNNED         = 0x00000008,                 // Aura::HandleAuraModStun
    UNIT_STAT_ROOT            = 0x00000010,                 // Aura::HandleAuraModRoot
    UNIT_STAT_ISOLATED        = 0x00000020,                 // area auras do not affect other players, Aura::HandleAuraModSchoolImmunity
    UNIT_STAT_CONTROLLED      = 0x00000040,                 // Aura::HandleAuraModPossess

    // persistent movement generator state (all time while movement generator applied to unit (independent from top state of movegen)
    UNIT_STAT_TAXI_FLIGHT     = 0x00000080,                 // player is in flight mode (in fact interrupted at far teleport until next map telport landing)
    UNIT_STAT_DISTRACTED      = 0x00000100,                 // DistractedMovementGenerator active

    // persistent movement generator state with non-persistent mirror states for stop support
    // (can be removed temporary by stop command or another movement generator apply)
    // not use _MOVE versions for generic movegen state, it can be removed temporary for unit stop and etc
    UNIT_STAT_CONFUSED        = 0x00000200,                 // ConfusedMovementGenerator active/onstack
    UNIT_STAT_CONFUSED_MOVE   = 0x00000400,
    UNIT_STAT_ROAMING         = 0x00000800,                 // RandomMovementGenerator/PointMovementGenerator/WaypointMovementGenerator active (now always set)
    UNIT_STAT_ROAMING_MOVE    = 0x00001000,
    UNIT_STAT_CHASE           = 0x00002000,                 // ChaseMovementGenerator active
    UNIT_STAT_CHASE_MOVE      = 0x00004000,
    UNIT_STAT_FOLLOW          = 0x00008000,                 // FollowMovementGenerator active
    UNIT_STAT_FOLLOW_MOVE     = 0x00010000,
    UNIT_STAT_FLEEING         = 0x00020000,                 // FleeMovementGenerator/TimedFleeingMovementGenerator active/onstack
    UNIT_STAT_FLEEING_MOVE    = 0x00040000,
    // More room for other MMGens

    // High-Level states (usually only with Creatures)
    UNIT_STAT_NO_COMBAT_MOVEMENT    = 0x01000000,           // Combat Movement for MoveChase stopped
    UNIT_STAT_RUNNING               = 0x02000000,           // SetRun for waypoints and such
    UNIT_STAT_WAYPOINT_PAUSED       = 0x04000000,           // Waypoint-Movement paused genericly (ie by script)

    UNIT_STAT_IGNORE_PATHFINDING    = 0x10000000,           // do not use pathfinding in any MovementGenerator

    // masks (only for check)

    // can't move currently
    UNIT_STAT_CAN_NOT_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED,

    // stay by different reasons
    UNIT_STAT_NOT_MOVE        = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_DISTRACTED,

    // stay or scripted movement for effect( = in player case you can't move by client command)
    UNIT_STAT_NO_FREE_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_TAXI_FLIGHT |
    UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING,

    // not react at move in sight or other
    UNIT_STAT_CAN_NOT_REACT   = UNIT_STAT_STUNNED | UNIT_STAT_DIED |
    UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING,

    // AI disabled by some reason
    UNIT_STAT_LOST_CONTROL    = UNIT_STAT_FLEEING | UNIT_STAT_CONTROLLED,

    // above 2 state cases
    UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL  = UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_LOST_CONTROL,

    // masks (for check or reset)

    // for real move using movegen check and stop (except unstoppable flight)
    UNIT_STAT_MOVING          = UNIT_STAT_ROAMING_MOVE | UNIT_STAT_CHASE_MOVE | UNIT_STAT_FOLLOW_MOVE | UNIT_STAT_FLEEING_MOVE,

    UNIT_STAT_RUNNING_STATE   = UNIT_STAT_CHASE_MOVE | UNIT_STAT_FLEEING_MOVE | UNIT_STAT_RUNNING,

    UNIT_STAT_ALL_STATE       = 0xFFFFFFFF,
    UNIT_STAT_ALL_DYN_STATES  = UNIT_STAT_ALL_STATE & ~(UNIT_STAT_NO_COMBAT_MOVEMENT | UNIT_STAT_RUNNING | UNIT_STAT_WAYPOINT_PAUSED | UNIT_STAT_IGNORE_PATHFINDING)
};

enum UnitMoveType
{
    MOVE_WALK           = 0,
    MOVE_RUN            = 1,
    MOVE_RUN_BACK       = 2,
    MOVE_SWIM           = 3,
    MOVE_SWIM_BACK      = 4,
    MOVE_TURN_RATE      = 5
};

#define MAX_MOVE_TYPE     6

extern float baseMoveSpeed[MAX_MOVE_TYPE];

/// internal used flags for marking special auras - for example some dummy-auras
enum UnitAuraFlags
{
    UNIT_AURAFLAG_ALIVE_INVISIBLE   = 0x1                   // aura which makes unit invisible for alive
};

enum UnitVisibility
{
    VISIBILITY_OFF                = 0,                      // absolute, not detectable, GM-like, can see all other
    VISIBILITY_ON                 = 1,
    VISIBILITY_GROUP_STEALTH      = 2,                      // detect chance, seen and can see group members
    VISIBILITY_GROUP_INVISIBILITY = 3,                      // invisibility, can see and can be seen only another invisible unit or invisible detection unit, set only if not stealthed, and in checks not used (mask used instead)
    VISIBILITY_GROUP_NO_DETECT    = 4,                      // state just at stealth apply for update Grid state. Don't remove, otherwise stealth spells will break
    VISIBILITY_REMOVE_CORPSE      = 5                       // special totally not detectable visibility for force delete object while removing a corpse
};

/**
 * [-ZERO] Need recheck values
 * Value masks for UNIT_FIELD_FLAGS (Taken from source)
 * \todo Document all the flags, not just the ones already commented
 */
enum UnitFlags
{
    UNIT_FLAG_NONE                  = 0x00000000,
    UNIT_FLAG_UNK_0                 = 0x00000001,
    UNIT_FLAG_NON_ATTACKABLE        = 0x00000002,           ///< not attackable
    UNIT_FLAG_DISABLE_MOVE          = 0x00000004,
    UNIT_FLAG_PVP_ATTACKABLE        = 0x00000008,           ///< allow apply pvp rules to attackable state in addition to faction dependent state, UNIT_FLAG_UNKNOWN1 in pre-bc mangos
    UNIT_FLAG_RENAME                = 0x00000010,           ///< rename creature
    UNIT_FLAG_RESTING               = 0x00000020,
    UNIT_FLAG_UNK_6                 = 0x00000040,
    UNIT_FLAG_OOC_NOT_ATTACKABLE    = 0x00000100,           ///< (OOC Out Of Combat) Can not be attacked when not in combat. Removed if unit for some reason enter combat (flag probably removed for the attacked and it's party/group only) \todo Needs more documentation
    UNIT_FLAG_PASSIVE               = 0x00000200,           ///< makes you unable to attack everything. Almost identical to our "civilian"-term. Will ignore it's surroundings and not engage in combat unless "called upon" or engaged by another unit.
    UNIT_FLAG_PVP                   = 0x00001000,
    UNIT_FLAG_SILENCED              = 0x00002000,           ///< silenced, 2.1.1
    UNIT_FLAG_UNK_14                = 0x00004000,           ///< 2.0.8
    UNIT_FLAG_UNK_15                = 0x00008000,           ///< related to jerky movement in water?
    UNIT_FLAG_UNK_16                = 0x00010000,           ///< removes attackable icon
    UNIT_FLAG_PACIFIED              = 0x00020000,
    UNIT_FLAG_DISABLE_ROTATE        = 0x00040000,
    UNIT_FLAG_IN_COMBAT             = 0x00080000,
    UNIT_FLAG_NOT_SELECTABLE        = 0x02000000,
    UNIT_FLAG_SKINNABLE             = 0x04000000,
    UNIT_FLAG_AURAS_VISIBLE         = 0x08000000,           ///< magic detect
    UNIT_FLAG_SHEATHE               = 0x40000000,
    // UNIT_FLAG_UNK_31              = 0x80000000           // no affect in 1.12.1

    // [-ZERO] TBC enumerations [?]
    UNIT_FLAG_NOT_ATTACKABLE_1      = 0x00000080,           ///< ?? (UNIT_FLAG_PVP_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1) is NON_PVP_ATTACKABLE
    UNIT_FLAG_LOOTING               = 0x00000400,           ///< loot animation
    UNIT_FLAG_PET_IN_COMBAT         = 0x00000800,           ///< in combat?, 2.0.8
    UNIT_FLAG_STUNNED               = 0x00040000,           ///< stunned, 2.1.1
    UNIT_FLAG_TAXI_FLIGHT           = 0x00100000,           ///< disable casting at client side spell not allowed by taxi flight (mounted?), probably used with 0x4 flag
    UNIT_FLAG_DISARMED              = 0x00200000,           ///< disable melee spells casting..., "Required melee weapon" added to melee spells tooltip.
    UNIT_FLAG_CONFUSED              = 0x00400000,
    UNIT_FLAG_FLEEING               = 0x00800000,
    UNIT_FLAG_PLAYER_CONTROLLED     = 0x01000000,           ///< used in spell Eyes of the Beast for pet... let attack by controlled creature
    // [-ZERO] UNIT_FLAG_MOUNT                 = 0x08000000,
    UNIT_FLAG_UNK_28                = 0x10000000,
    UNIT_FLAG_UNK_29                = 0x20000000            ///< used in Feign Death spell
};

/**
 * Non Player Character flags
 * \todo Properly document this
 */
enum NPCFlags
{
    UNIT_NPC_FLAG_NONE                  = 0x00000000,
    UNIT_NPC_FLAG_GOSSIP                = 0x00000001,       ///< 100%
    UNIT_NPC_FLAG_QUESTGIVER            = 0x00000002,       ///< guessed, probably ok
    UNIT_NPC_FLAG_VENDOR                = 0x00000004,       ///< 100%
    UNIT_NPC_FLAG_FLIGHTMASTER          = 0x00000008,       ///< 100%
    UNIT_NPC_FLAG_TRAINER               = 0x00000010,       ///< 100%
    UNIT_NPC_FLAG_SPIRITHEALER          = 0x00000020,       ///< guessed
    UNIT_NPC_FLAG_SPIRITGUIDE           = 0x00000040,       ///< guessed
    UNIT_NPC_FLAG_INNKEEPER             = 0x00000080,       ///< 100%
    UNIT_NPC_FLAG_BANKER                = 0x00000100,       ///< 100%
    UNIT_NPC_FLAG_PETITIONER            = 0x00000200,       ///< 100% 0xC0000 = guild petitions
    UNIT_NPC_FLAG_TABARDDESIGNER        = 0x00000400,       ///< 100%
    UNIT_NPC_FLAG_BATTLEMASTER          = 0x00000800,       ///< 100%
    UNIT_NPC_FLAG_AUCTIONEER            = 0x00001000,       ///< 100%
    UNIT_NPC_FLAG_STABLEMASTER          = 0x00002000,       ///< 100%
    UNIT_NPC_FLAG_REPAIR                = 0x00004000,       ///< 100%
    UNIT_NPC_FLAG_OUTDOORPVP            = 0x20000000        ///< custom flag for outdoor pvp creatures || Custom flag
};

/**
 * These flags denote the different kinds of movement you can do. You can have many at the
 * same time as this is used as a bitmask.
 * \todo [-ZERO] Need check and update used in most movement packets (send and received)
 * \see MovementInfo
 */
enum MovementFlags
{
    MOVEFLAG_NONE             = 0x00000000,
    MOVEFLAG_FORWARD          = 0x00000001,
    MOVEFLAG_BACKWARD         = 0x00000002,
    MOVEFLAG_STRAFE_LEFT      = 0x00000004,
    MOVEFLAG_STRAFE_RIGHT     = 0x00000008,
    MOVEFLAG_TURN_LEFT        = 0x00000010,
    MOVEFLAG_TURN_RIGHT       = 0x00000020,
    MOVEFLAG_PITCH_UP         = 0x00000040,
    MOVEFLAG_PITCH_DOWN       = 0x00000080,
    MOVEFLAG_WALK_MODE        = 0x00000100,               // Walking

    MOVEFLAG_LEVITATING       = 0x00000400,
    MOVEFLAG_FLYING           = 0x00000800,               // [-ZERO] is it really need and correct value
    MOVEFLAG_FALLING          = 0x00002000,
    MOVEFLAG_FALLINGFAR       = 0x00004000,
    MOVEFLAG_SWIMMING         = 0x00200000,               // appears with fly flag also
    MOVEFLAG_SPLINE_ENABLED   = 0x00400000,
    MOVEFLAG_CAN_FLY          = 0x00800000,               // [-ZERO] is it really need and correct value
    MOVEFLAG_FLYING_OLD       = 0x01000000,               // [-ZERO] is it really need and correct value

    MOVEFLAG_ONTRANSPORT      = 0x02000000,               // Used for flying on some creatures
    MOVEFLAG_SPLINE_ELEVATION = 0x04000000,               // used for flight paths
    MOVEFLAG_ROOT             = 0x08000000,               // used for flight paths
    MOVEFLAG_WATERWALKING     = 0x10000000,               // prevent unit from falling through water
    MOVEFLAG_SAFE_FALL        = 0x20000000,               // active rogue safe fall spell (passive)
    MOVEFLAG_HOVER            = 0x40000000
};

// flags that use in movement check for example at spell casting
MovementFlags const movementFlagsMask = MovementFlags(
    MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD | MOVEFLAG_STRAFE_LEFT | MOVEFLAG_STRAFE_RIGHT |
    MOVEFLAG_PITCH_UP | MOVEFLAG_PITCH_DOWN | MOVEFLAG_ROOT |
    MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR | MOVEFLAG_SPLINE_ELEVATION
    );

MovementFlags const movementOrTurningFlagsMask = MovementFlags(
            movementFlagsMask | MOVEFLAG_TURN_LEFT | MOVEFLAG_TURN_RIGHT
        );

class MovementInfo
{
    public:
        MovementInfo() : moveFlags(MOVEFLAG_NONE), time(0),
            t_time(0), s_pitch(0.0f), fallTime(0), u_unk1(0.0f) {}

        // Read/Write methods
        void Read(ByteBuffer& data);
        void Write(ByteBuffer& data) const;

        // Movement flags manipulations
        void AddMovementFlag(MovementFlags f) { moveFlags |= f; }
        void RemoveMovementFlag(MovementFlags f) { moveFlags &= ~f; }
        bool HasMovementFlag(MovementFlags f) const { return moveFlags & f; }
        MovementFlags GetMovementFlags() const { return MovementFlags(moveFlags); }
        void SetMovementFlags(MovementFlags f) { moveFlags = f; }

        // Position manipulations
        Position const* GetPos() const { return &pos; }
        void SetTransportData(ObjectGuid guid, float x, float y, float z, float o, uint32 time)
        {
            t_guid = guid;
            t_pos.x = x;
            t_pos.y = y;
            t_pos.z = z;
            t_pos.o = o;
            t_time = time;
        }
        void ClearTransportData()
        {
            t_guid = ObjectGuid();
            t_pos.x = 0.0f;
            t_pos.y = 0.0f;
            t_pos.z = 0.0f;
            t_pos.o = 0.0f;
            t_time = 0;
        }
        ObjectGuid const& GetTransportGuid() const { return t_guid; }
        Position const* GetTransportPos() const { return &t_pos; }
        uint32 GetTime() { return time; }
        uint32 GetTransportTime() const { return t_time; }
        uint32 GetFallTime() const { return fallTime; }
        void ChangeOrientation(float o) { pos.o = o; }
        void ChangePosition(float x, float y, float z, float o) { pos.x = x; pos.y = y; pos.z = z; pos.o = o; }
        void UpdateTime(uint32 _time) { time = _time; }

        struct JumpInfo
        {
            JumpInfo() : velocity(0.f), sinAngle(0.f), cosAngle(0.f), xyspeed(0.f) {}
            float   velocity, sinAngle, cosAngle, xyspeed;
        };

        JumpInfo const& GetJumpInfo() const { return jump; }
    private:
        // common
        uint32   moveFlags;                                 // see enum MovementFlags
        uint32   time;
        Position pos;
        // transport
        ObjectGuid t_guid;
        Position t_pos;
        uint32   t_time;
        // swimming and unknown
        float    s_pitch;
        // last fall time
        uint32   fallTime;
        // jumping
        JumpInfo jump;
        // spline
        float    u_unk1;
};

inline ByteBuffer& operator<< (ByteBuffer& buf, MovementInfo const& mi)
{
    mi.Write(buf);
    return buf;
}

inline ByteBuffer& operator>> (ByteBuffer& buf, MovementInfo& mi)
{
    mi.Read(buf);
    return buf;
}

namespace Movement
{
    class MoveSpline;
}

/**
 * The different available diminishing return levels.
 * \see DiminishingReturn
 */
enum DiminishingLevels
{
    DIMINISHING_LEVEL_1             = 0,         ///<Won't make a difference to stun duration
    DIMINISHING_LEVEL_2             = 1,         ///<Reduces stun time by 50%
    DIMINISHING_LEVEL_3             = 2,         ///<Reduces stun time by 75%
    DIMINISHING_LEVEL_IMMUNE        = 3          ///<The target is immune to the DiminishingGrouop
};

/**
 * Structure to keep track of diminishing returns, for more information
 * about the idea behind diminishing returns, see: http://www.wowwiki.com/Diminishing_returns
 * \see Unit::GetDiminishing
 * \see Unit::IncrDiminishing
 * \see Unit::ApplyDiminishingToDuration
 * \see Unit::ApplyDiminishingAura
 */
struct DiminishingReturn
{
    DiminishingReturn(DiminishingGroup group, uint32 t, uint32 count)
        : DRGroup(group), stack(0), hitTime(t), hitCount(count)
    {}

    /**
     * Group that this diminishing return will affect
     */
    DiminishingGroup        DRGroup: 16;
    /**
     * Seems to be how many times this has been stacked, modified in
     * Unit::ApplyDiminishingAura
     */
    uint16                  stack: 16;
    /**
     * Records at what time the last hit with this DiminishingGroup was done, if it's
     * higher than 15 seconds (ie: 15 000 ms) the DiminishingReturn::hitCount will be reset
     * to DiminishingLevels::DIMINISHING_LEVEL_1, which will do no difference to the duration
     * of the stun etc.
     */
    uint32                  hitTime;
    /**
     * Records how many times a spell of this DiminishingGroup has hit, this in turn
     * decides how how long the duration of the stun etc is.
     */
    uint32                  hitCount;
};

/**
 * At least some values expected fixed and used in auras field, other custom
 */
enum MeleeHitOutcome
{
    MELEE_HIT_EVADE     = 0,
    MELEE_HIT_MISS      = 1,
    MELEE_HIT_DODGE     = 2,     ///< used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_BLOCK     = 3,     ///< used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_PARRY     = 4,     ///< used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_GLANCING  = 5,
    MELEE_HIT_CRIT      = 6,
    MELEE_HIT_CRUSHING  = 7,
    MELEE_HIT_NORMAL    = 8,
    MELEE_HIT_BLOCK_CRIT = 9,
};

struct CleanDamage
{
    CleanDamage(uint32 _damage, WeaponAttackType _attackType, MeleeHitOutcome _hitOutCome) :
        damage(_damage), attackType(_attackType), hitOutCome(_hitOutCome) {}

    uint32 damage;
    WeaponAttackType attackType;
    MeleeHitOutcome hitOutCome;
};

/**
 * Struct for use in Unit::CalculateMeleeDamage
 * Need create structure like in SMSG_ATTACKERSTATEUPDATE opcode
 */
struct CalcDamageInfo
{
    /// Attacker
    Unit*  attacker;
    /// Target for damage
    Unit*  target;
    SpellSchoolMask damageSchoolMask;
    /// How much damage was actually done
    uint32 damage;
    /// How much damage that was absorbed
    uint32 absorb;
    /// How much of the damage that was resisted
    uint32 resist;
    /// How much of the damage that was blocked
    uint32 blocked_amount;
    /**
     * Bitmask of the possible HitInfo flags
     * \see HitInfo
     */
    uint32 HitInfo;
    /**
     * What state the target is in, ie: is he evading or deflecting the hit?
     * \see VictimState
     */
    uint32 TargetState;
    /**
     * Tells how the target was attacked
     */
    WeaponAttackType attackType;
    /**
     * Proc flags of the attacker that should have a chance to trigger, ie: successful
     * melee hit
     * \see ProcFlags
     */
    uint32 procAttacker;
    /**
     * Proc flags of the victim that should have a change to trigger, ie: successful
     * block
     * \see ProcFlags
     */
    uint32 procVictim;
    /**
     * Extra proc flags?
     * \todo Used for what?
     */
    uint32 procEx;
    /// Used only for rage calculation
    uint32 cleanDamage;
    /// (Old comment) \todo remove this field (need use TargetState)
    MeleeHitOutcome hitOutCome;
};

/**
 * Spell damage info structure based on structure sending in SMSG_SPELLNONMELEEDAMAGELOG opcode
 */
struct SpellNonMeleeDamage
{
    SpellNonMeleeDamage(Unit* _attacker, Unit* _target, uint32 _SpellID, SpellSchools _school)
        : target(_target), attacker(_attacker), SpellID(_SpellID), damage(0), school(_school),
          absorb(0), resist(0), physicalLog(false), unused(false), blocked(0), HitInfo(0)
    {}

    Unit*   target;
    Unit*   attacker;
    uint32 SpellID;
    uint32 damage;
    SpellSchools school;
    uint32 absorb;
    uint32 resist;
    bool   physicalLog;
    bool   unused;
    uint32 blocked;
    uint32 HitInfo;
};

/**
 * Used as a convenience struct for the \ref Unit::SendPeriodicAuraLog
 * \todo Is it used in more places? Check SpellAuras.cpp for some examples and document it
 */
struct SpellPeriodicAuraLogInfo
{
    SpellPeriodicAuraLogInfo(Aura* _aura, uint32 _damage, uint32 _absorb, uint32 _resist, float _multiplier)
        : aura(_aura), damage(_damage), absorb(_absorb), resist(_resist), multiplier(_multiplier) {}

    Aura*   aura;       ///< The \ref Aura in question
    uint32 damage;      ///< How much damage this does or how much it adds if it's a positive \ref Aura
    uint32 absorb;      ///< The amount that was absorbed
    uint32 resist;      ///< The amount that was resisted
    float  multiplier;  ///< The multiplier for gain, ie if it's higher you gain more probably
};

uint32 createProcExtendMask(SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition);

enum SpellAuraProcResult
{
    SPELL_AURA_PROC_OK              = 0,                    // proc was processed, will remove charges
    SPELL_AURA_PROC_FAILED          = 1,                    // proc failed - if at least one aura failed the proc, charges won't be taken
    SPELL_AURA_PROC_CANT_TRIGGER    = 2                     // aura can't trigger - skip charges taking, move to next aura if exists
};

typedef SpellAuraProcResult(Unit::*pAuraProcHandler)(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
extern pAuraProcHandler AuraProcHandler[TOTAL_AURAS];

enum CurrentSpellTypes
{
    CURRENT_MELEE_SPELL             = 0,
    CURRENT_GENERIC_SPELL           = 1,
    CURRENT_AUTOREPEAT_SPELL        = 2,
    CURRENT_CHANNELED_SPELL         = 3
};

#define CURRENT_FIRST_NON_MELEE_SPELL 1
#define CURRENT_MAX_SPELL             4

struct GlobalCooldown
{
    explicit GlobalCooldown(uint32 _dur = 0, uint32 _time = 0) : duration(_dur), cast_time(_time) {}

    uint32 duration;
    uint32 cast_time;
};

typedef UNORDERED_MAP < uint32 /*category*/, GlobalCooldown > GlobalCooldownList;

class GlobalCooldownMgr                                     // Shared by Player and CharmInfo
{
    public:
        GlobalCooldownMgr() {}

    public:
        bool HasGlobalCooldown(SpellEntry const* spellInfo) const;
        void AddGlobalCooldown(SpellEntry const* spellInfo, uint32 gcd);
        void CancelGlobalCooldown(SpellEntry const* spellInfo);

    private:
        GlobalCooldownList m_GlobalCooldowns;
};

enum ActiveStates
{
    ACT_PASSIVE  = 0x01,                                    // 0x01 - passive
    ACT_DISABLED = 0x81,                                    // 0x80 - castable
    ACT_ENABLED  = 0xC1,                                    // 0x40 | 0x80 - auto cast + castable
    ACT_COMMAND  = 0x07,                                    // 0x01 | 0x02 | 0x04
    ACT_REACTION = 0x06,                                    // 0x02 | 0x04
    ACT_DECIDE   = 0x00                                     // custom
};

enum ReactStates
{
    REACT_PASSIVE    = 0,
    REACT_DEFENSIVE  = 1,
    REACT_AGGRESSIVE = 2
};

enum CommandStates
{
    COMMAND_STAY    = 0,
    COMMAND_FOLLOW  = 1,
    COMMAND_ATTACK  = 2,
    COMMAND_ABANDON = 3
};

#define UNIT_ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define UNIT_ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_UNIT_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)
#define MAKE_UNIT_ACTION_BUTTON(A,T) (uint32(A) | (uint32(T) << 24))

struct UnitActionBarEntry
{
    UnitActionBarEntry() : packedData(uint32(ACT_DISABLED) << 24) {}

    uint32 packedData;

    // helper
    ActiveStates GetType() const { return ActiveStates(UNIT_ACTION_BUTTON_TYPE(packedData)); }
    uint32 GetAction() const { return UNIT_ACTION_BUTTON_ACTION(packedData); }
    bool IsActionBarForSpell() const
    {
        ActiveStates Type = GetType();
        return Type == ACT_DISABLED || Type == ACT_ENABLED || Type == ACT_PASSIVE;
    }

    void SetActionAndType(uint32 action, ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(action, type);
    }

    void SetType(ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(UNIT_ACTION_BUTTON_ACTION(packedData), type);
    }

    void SetAction(uint32 action)
    {
        packedData = (packedData & 0xFF000000) | UNIT_ACTION_BUTTON_ACTION(action);
    }
};

typedef UnitActionBarEntry CharmSpellEntry;

enum ActionBarIndex
{
    ACTION_BAR_INDEX_START = 0,
    ACTION_BAR_INDEX_PET_SPELL_START = 3,
    ACTION_BAR_INDEX_PET_SPELL_END = 7,
    ACTION_BAR_INDEX_END = 10,
};

#define MAX_UNIT_ACTION_BAR_INDEX (ACTION_BAR_INDEX_END-ACTION_BAR_INDEX_START)

/**
 * This structure/class is used when someone is charming (ie: mind control spell and the like)
 * someone else, to get the charmed ones action bar, the spells and such. It also takes care
 * of pets the charmed one has etc. 
 */
struct CharmInfo
{
    public:
        explicit CharmInfo(Unit* unit);
        uint32 GetPetNumber() const { return m_petnumber; }
        void SetPetNumber(uint32 petnumber, bool statwindow);

        void SetCommandState(CommandStates st) { m_CommandState = st; }
        CommandStates GetCommandState() { return m_CommandState; }
        bool HasCommandState(CommandStates state) { return (m_CommandState == state); }
        void SetReactState(ReactStates st) { m_reactState = st; }
        ReactStates GetReactState() { return m_reactState; }
        bool HasReactState(ReactStates state) { return (m_reactState == state); }

        void InitPossessCreateSpells();
        void InitCharmCreateSpells();
        void InitPetActionBar();
        void InitEmptyActionBar();

        // return true if successful
        bool AddSpellToActionBar(uint32 spellid, ActiveStates newstate = ACT_DECIDE);
        bool RemoveSpellFromActionBar(uint32 spell_id);
        void LoadPetActionBar(const std::string& data);
        void BuildActionBar(WorldPacket* data);
        void SetSpellAutocast(uint32 spell_id, bool state);
        void SetActionBar(uint8 index, uint32 spellOrAction, ActiveStates type)
        {
            PetActionBar[index].SetActionAndType(spellOrAction, type);
        }
        UnitActionBarEntry const* GetActionBarEntry(uint8 index) const { return &(PetActionBar[index]); }

        void ToggleCreatureAutocast(uint32 spellid, bool apply);

        CharmSpellEntry* GetCharmSpell(uint8 index) { return &(m_charmspells[index]); }

        GlobalCooldownMgr& GetGlobalCooldownMgr() { return m_GlobalCooldownMgr; }

    private:
        Unit* m_unit;
        UnitActionBarEntry PetActionBar[MAX_UNIT_ACTION_BAR_INDEX];
        CharmSpellEntry m_charmspells[CREATURE_MAX_SPELLS];
        CommandStates   m_CommandState;
        ReactStates     m_reactState;
        uint32          m_petnumber;
        GlobalCooldownMgr m_GlobalCooldownMgr;
};

// used in CallForAllControlledUnits/CheckAllControlledUnits
enum ControlledUnitMask
{
    CONTROLLED_PET       = 0x01,
    CONTROLLED_MINIPET   = 0x02,
    CONTROLLED_GUARDIANS = 0x04,                            // including PROTECTOR_PET
    CONTROLLED_CHARM     = 0x08,
    CONTROLLED_TOTEMS    = 0x10,
};

// for clearing special attacks
#define REACTIVE_TIMER_START 4000

enum ReactiveType
{
    REACTIVE_DEFENSE      = 1,
    REACTIVE_HUNTER_PARRY = 2,
    // REACTIVE_CRIT         = 3,
    // REACTIVE_HUNTER_CRIT  = 4,
    REACTIVE_OVERPOWER    = 5
};

#define MAX_REACTIVE 6

// delay time next attack to prevent client attack animation problems
#define ATTACK_DISPLAY_DELAY 200
#define MAX_PLAYER_STEALTH_DETECT_RANGE 45.0f               // max distance for detection targets by player
#define MAX_CREATURE_ATTACK_RADIUS 45.0f                    // max distance for creature aggro (use with CONFIG_FLOAT_RATE_CREATURE_AGGRO)

// Regeneration defines
#define REGEN_TIME_FULL     2000                            // For this time difference is computed regen value

// Power type values defines
enum PowerDefaults
{
    POWER_RAGE_DEFAULT              = 1000,
    POWER_FOCUS_DEFAULT             = 100,
    POWER_ENERGY_DEFAULT            = 100,
    POWER_HAPPINESS_DEFAULT         = 1000000,
};

struct SpellProcEventEntry;                                 // used only privately

class Unit : public WorldObject
{
    public:
        typedef std::set<Unit*> AttackerSet;
        /**
         * A multimap from spell ids to \ref SpellAuraHolder, multiple \ref SpellAuraHolder can have
         * the same id (ie: the same key)
         */
        typedef std::multimap < uint32 /*spellId*/, SpellAuraHolder* > SpellAuraHolderMap;
        /**
         * A pair of two iterators to a \ref SpellAuraHolderMap which is used in conjunction
         * with the std::multimap::equal_range which gives all \ref SpellAuraHolder that have the same
         * spellid in this case, the first member is the iterator to the beginning, and the
         * second member is the iterator to the end.
         */
        typedef std::pair<SpellAuraHolderMap::iterator, SpellAuraHolderMap::iterator> SpellAuraHolderBounds;
        /// Same thing as \ref SpellAuraHolderBounds but with const_iterator instead of iterator
        typedef std::pair<SpellAuraHolderMap::const_iterator, SpellAuraHolderMap::const_iterator> SpellAuraHolderConstBounds;
        typedef std::list<SpellAuraHolder*> SpellAuraHolderList;
        /**
         * List of \ref Aura used in \ref Unit::GetAurasByType and more and also in the members
         * \ref Unit::m_modAuras and \ref Unit::m_deletedAuras
         * \see Aura
         */
        typedef std::list<Aura*> AuraList;
        /**
         * List of \ref DiminishingReturn used for calculation of the same thing.
         * \see DiminishingReturn
         * \see DiminishingLevels
         * \see Unit::GetDiminishing
         * \see Unit::IncrDiminishing
         * \see Unit::ApplyDiminishingToDuration
         */
        typedef std::list<DiminishingReturn> Diminishing;
        typedef std::set < uint32 /*playerGuidLow*/ > ComboPointHolderSet;
        typedef std::map < SpellEntry const*, ObjectGuid /*targetGuid*/ > TrackedAuraTargetMap;

        virtual ~Unit();

        //These are probably interesting for learning how/when objects/units get added to the
        //world, take a look at them and write something tutorialishy about it to introduce
        //more of how the core works?
        void AddToWorld() override;
        void RemoveFromWorld() override;

        /**
         * Used in ~Creature/~Player (or before mass creature delete to remove
         * cross-references to already deleted units). (Taken from comment in source)
         * \todo Add more information here
         */
        void CleanupsBeforeDelete() override;

        float GetObjectBoundingRadius() const override      // overwrite WorldObject version
        {
            return m_floatValues[UNIT_FIELD_BOUNDINGRADIUS];
        }

        /**
         * Gets the current DiminishingLevels for the given group
         * @param group The group that you would like to know the current diminishing return level for
         * @return The current diminishing level, up to DiminishingLevels::DIMINISHING_LEVEL_IMMUNE
         */
        DiminishingLevels GetDiminishing(DiminishingGroup  group);
        /**
         * Increases the level of the DiminishingGroup by one level up until
         * DIMINISHING_LEVEL_IMMUNE where the target becomes immune to spells of
         * that DiminishingGroup
         * @param group The group to increase the level for by one
         */
        void IncrDiminishing(DiminishingGroup group);
        /**
         * Calculates how long the duration of a spell should be considering
         * diminishing returns, ie, if the Level passed in is DIMINISHING_LEVEL_IMMUNE
         * then the duration will be zeroed out. If it is DIMINISHING_LEVEL_1 then a full
         * duration will be used
         * @param group The group to affect
         * @param duration The duration to be changed, will be updated with the new duration
         * @param caster Who's casting the spell, used to decide whether anything should be calculated
         * @param Level The current level of diminishing returns for the group, decides the new duration
         * @param isReflected Whether the spell was reflected or not, used to determine if we should do any calculations at all.
         */
        void ApplyDiminishingToDuration(DiminishingGroup  group, int32& duration, Unit* caster, DiminishingLevels Level, bool isReflected);
        /**
         * Applies a diminishing return to the given group if apply is true,
         * otherwise lowers the level by one (?)
         * @param group The group to affect
         * @param apply whether this aura is being added/removed
         */
        void ApplyDiminishingAura(DiminishingGroup  group, bool apply);
        /**
         * Clears all the current diminishing returns for this Unit.
         */
        void ClearDiminishings() { m_Diminishing.clear(); }

        void Update(uint32 update_diff, uint32 time) override;

        /**
         * Updates the attack time for the given WeaponAttackType
         * @param type The type of weapon that we want to update the time for
         * @param time the remaining time until we can attack with the WeaponAttackType again
         */
        void setAttackTimer(WeaponAttackType type, uint32 time) { m_attackTimer[type] = time; }
        /**
         * Resets the attack timer to the base value decided by Unit::m_modAttackSpeedPct and
         * Unit::GetAttackTime
         * @param type The weapon attack type to reset the attack timer for.
         */
        void resetAttackTimer(WeaponAttackType type = BASE_ATTACK);
        /**
         * Get's the remaining time until we can do an attack
         * @param type The weapon type to check the remaining time for
         * @return The remaining time until we can attack with this weapon type.
         */
        uint32 getAttackTimer(WeaponAttackType type) const { return m_attackTimer[type]; }
        /**
         * Checks whether the unit can do an attack. Does this by checking the attacktimer for the
         * WeaponAttackType, can probably be thought of as a cooldown for each swing/shot
         * @param type What weapon should we check for
         * @return true if the Unit::m_attackTimer is zero for the given WeaponAttackType
         */
        bool isAttackReady(WeaponAttackType type = BASE_ATTACK) const { return m_attackTimer[type] == 0; }
        /**
         * Checks if the current Unit has an offhand weapon
         * @return True if there is a offhand weapon.
         */
        bool haveOffhandWeapon() const;
        /**
         * Does an attack if any of the timers allow it and resets them, if the user
         * isn't in range or behind the target an error is sent to the client.
         * Also makes sure to not make and offhand and mainhand attack at the same
         * time. Only handles non-spells ie melee attacks.
         * @return True if an attack was made and no error happened, false otherwise
         */
        bool UpdateMeleeAttackingState();
        /**
         * Check is a given equipped weapon can be used, ie the mainhand, offhand etc.
         * @param attackType The attack type to check, ie: main/offhand/ranged
         * @return True if the weapon can be used, true except for shapeshifts and if disarmed.
         */
        bool CanUseEquippedWeapon(WeaponAttackType attackType) const
        {
            if (IsInFeralForm())
                { return false; }

            switch (attackType)
            {
                default:
                case BASE_ATTACK:
                    return !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISARMED);
                case OFF_ATTACK:
                case RANGED_ATTACK:
                    return true;
            }
        }

        /**
         * Returns the combined combat reach of two mobs. Can be seen as a radius.
         * @param pVictim The other unit to add the range for
         * @param forMeleeRange Whether we should return the combined reach for melee or not
         * @param flat_mod Increases the returned reach by this value.
         * @return The combined values of UNIT_FIELD_COMBATREACH for both this unit and the pVictim.
         * \see EUnitFields
         * \see GetFloatValue
         */
        float GetCombatReach(Unit const* pVictim, bool forMeleeRange = true, float flat_mod = 0.0f) const;
        /**
         * Returns the remaining combat distance between two mobs (CombatReach substracted).
         * Does this by getting the radius of combat/aggro between them and then subtracting their
         * actual distance between them. Ie: dist between - radius for aggro. If this becomes less
         * than zero zero is returned and the mobs should probably aggro each other/the player
         * @param target The target to check against
         * @param forMeleeRange If we want to check melee range instead
         * @return The reach between them left until one of the creatures could/should aggro
         */
        float GetCombatDistance(Unit const* target, bool forMeleeRange) const;
        /**
         * Returns if the Unit can reach a victim with Melee Attack. Does so by using
         * Unit::GetCombatReach for melee and checking if the distance from the target is less than
         * the reach.
         * @param pVictim Who we want to reach with a melee attack.
         * @param flat_mod The same as sent to Unit::GetCombatReach
         * @return true if we can reach pVictim with a melee attack
         */
        bool CanReachWithMeleeAttack(Unit const* pVictim, float flat_mod = 0.0f) const;
        uint32 m_extraAttacks;

        /**
         * Internal function, must only be called from Unit::Attack(Unit*)
         * @param pAttacker The attacker to add to current attackers.
         */
        void _addAttacker(Unit* pAttacker)                  // must be called only from Unit::Attack(Unit*)
        {
            AttackerSet::const_iterator itr = m_attackers.find(pAttacker);
            if (itr == m_attackers.end())
                { m_attackers.insert(pAttacker); }
        }
        /**
         * Internal function, must only be called from Unit::AttackStop()
         * @param pAttacker
         */
        void _removeAttacker(Unit* pAttacker)               // must be called only from Unit::AttackStop()
        {
            m_attackers.erase(pAttacker);
        }
        /**
         * If another mob/unit want to help this mob this function will return a
         * possible Unit to attack.
         * @return A Unit to attack if this one is being attacked by anyone, NULL otherwise
         */
        Unit* getAttackerForHelper()                        // If someone wants to help, who to give them
        {
            if (getVictim() != NULL)
                { return getVictim(); }

            if (!m_attackers.empty())
                { return *(m_attackers.begin()); }

            return NULL;
        }
        /**
         * Tries to attack a Unit/Player, also makes sure to stop attacking the current target
         * if we're already attacking someone.
         * @param victim The Unit to attack
         * @param meleeAttack Whether we should attack with melee or ranged/magic
         * @return True if an attack was initiated, false otherwise
         */
        bool Attack(Unit* victim, bool meleeAttack);
        /**
         * Called when we are attack by someone in someway, might be when a fear runs out and
         * we want to notify AI to attack again or when a spell hits.
         * @param attacker Who's attacking us
         */
        void AttackedBy(Unit* attacker);
        /**
         * Stop all spells from casting except the one give by except_spellid
         * @param except_spellid This spell id will not be stopped from casting, defaults to 0
         * \see Unit::InterruptSpell
         */
        void CastStop(uint32 except_spellid = 0);
        /**
         * Stops attacking whatever we are attacking at the moment and tells the Unit we are attacking
         * that we are not doing that anymore, ie: removes it from the attacker list
         * @param targetSwitch if we are switching targets or not, defaults to false
         * @return false if we weren't attacking already, true otherwise
         * \see Unit::m_attacking
         */
        bool AttackStop(bool targetSwitch = false);
        /**
         * Removes all attackers from the Unit::m_attackers set and logs it if someone that
         * wasn't attacking it was in the list. Does this check by checking if Unit::AttackStop()
         * returned false.
         * \see Unit::AttackStop
         */
        void RemoveAllAttackers();
        /**
         * @return The Unit::m_attackers, ie. the units that are attacking you
         */
        AttackerSet const& getAttackers() const { return m_attackers; }
        /**
         * Checks if we are attacking a player, also, pets/minions etc attacking a player counts
         * towards you attacking a player.
         * @return true if you and/or your pets/minions etc are attacking a player.
         * \todo Rename to IsAttackingPlayer to follow naming conventions?
         */
        bool isAttackingPlayer() const;
        /**
         * @return The victim that you are currently attacking
         */
        Unit* getVictim() const { return m_attacking; }
        /**
         * Stops a unit from combat, removes all attackers and stops attacking.
         * @param includingCast if we should stop the currently casting spell aswell
         */
        void CombatStop(bool includingCast = false);
        /**
         * Calls Unit::CombatStop to stop combat, also calls Unit::CombatStop for pets etc. by using
         * Unit::CallForAllControlledUnits
         * @param includingCast if we should stop the currently casting spell aswell
         * \see Unit::CallForAllControlledUnits
         * \see Unit::CheckForAllControlledUnits
         */
        void CombatStopWithPets(bool includingCast = false);
        /**
         * Stops attacking a certain faction. If we are attacking something and are a player we
         * are forcefully stopped from attacking the target aswell.
         * @param faction_id The faction to stop attacking
         * \see Unit::CallForAllControlledUnits
         * \see Unit::CheckForAllControlledUnits
         * \see Unit::getAttackers
         */
        void StopAttackFaction(uint32 faction_id);
        /**
         * Selects a random unfriendly target, takes care of LOS and such aswell
         * @param except select any target but this one, usually your current target
         * @param radius how big the radius for our search should be
         * @return The random unfriendly target found, NULL if no targets were found
         * \see MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck
         * \see Mangos::UnitListSearcher
         * \see Cell::VisitAllObjects
         */
        Unit* SelectRandomUnfriendlyTarget(Unit* except = NULL, float radius = ATTACK_DISTANCE) const;
        /**
         * Same as Unit::SelectRandomUnfriendlyTarget except it selects a friendly target
         * @param except select any target but this one, usually your current target
         * @param radius how big the radius for our search should be
         * @return The random friendly target found, NULL if no targets were found
         * \see MaNGOS::AnyFriendlyUnitInObjectRangeCheck
         * \see Mangos::UnitListSearcher
         * \see Cell::VisitAllObjects
         */
        Unit* SelectRandomFriendlyTarget(Unit* except = NULL, float radius = ATTACK_DISTANCE) const;
        /**
         * Checks if we have a negative aura with the given interrupt flag/s
         * @param flag The interrupt flag/s to check for, see SpellAuraInterruptFlags
         * @return true if we have a negative aura with the given flag, false otherwise
         * \see SpellAuraInterruptFlags
         */
        bool hasNegativeAuraWithInterruptFlag(uint32 flag);
        /**
         * Sends a packet to the client informing it that melee attacks are stopping
         * @param victim The unit we stopped attacking
         * \see OpcodesList
         */
        void SendMeleeAttackStop(Unit* victim);
        /**
         * Sends a packet to the client informing it that melee attacks are starting
         * @param pVictim the target that we attack with melee
         */
        void SendMeleeAttackStart(Unit* pVictim);

        /**
         * Adds a state to this unit
         * @param f the state to add, see UnitState for possible values
         * \see UnitState
         */
        void addUnitState(uint32 f) { m_state |= f; }
        /**
         * Checks if a certain unit state is set
         * @param f the state to check for
         * @return true if the state is set, false otherwise
         * \see UnitState
         */
        bool hasUnitState(uint32 f) const { return (m_state & f); }
        /**
         * Unsets a certain unit state
         * @param f the state to remove
         * \see UnitState
         */
        void clearUnitState(uint32 f) { m_state &= ~f; }
        /**
         * Checks if the client/mob is in control or no
         * @return true if the client can move by client control, false otherwise
         * \see UnitState
         */
        bool CanFreeMove() const
        {
            return !hasUnitState(UNIT_STAT_NO_FREE_MOVE) && !GetOwnerGuid();
        }

        /**
         * Gets the level for this unit
         * @return The current level for this unit
         * \see GetUInt32Value
         * \see EUnitFields
         */
        uint32 getLevel() const { return GetUInt32Value(UNIT_FIELD_LEVEL); }
        /**
         * @return The level it would seem
         * \todo What does it actually do? Is overwritten by others that derive from Unit?
         */
        virtual uint32 GetLevelForTarget(Unit const* /*target*/) const { return getLevel(); }
        /**
         * Updates the level for the current Unit. Also updates the group to know about this.
         * @param lvl The level to change to
         * \see EUnitFields
         * \see SetUInt32Value
         */
        void SetLevel(uint32 lvl);
        /**
         * Gets the race of this Unit, not to be confused with the Creature type or such
         * @return returns the race of this Unit
         * \see CreatureTypeFlags
         * \see Races
         */
        uint8 getRace() const { return GetByteValue(UNIT_FIELD_BYTES_0, 0); }
        /**
         * Returns a bitmask representation of the current race given by Races, not to be
         * confused with the Creature type or such
         * @return the racemask for the current race
         * \see CreatureTypeFlags
         * \see Races
         */
        uint32 getRaceMask() const { return 1 << (getRace() - 1); }
        /**
         * Returns the class of this Unit
         * @return the class of the Unit
         * \see Classes
         */
        uint8 getClass() const { return GetByteValue(UNIT_FIELD_BYTES_0, 1); }
        /**
         * Returns a bitmask representation of the current class given by Classes
         * @return the classmask for the class
         * \see Classes
         */
        uint32 getClassMask() const { return 1 << (getClass() - 1); }
        /**
         * Gives you the current gender of this Unit
         * @return The current gender
         * \see Gender
         */
        uint8 getGender() const { return GetByteValue(UNIT_FIELD_BYTES_0, 2); }

        /**
         * Gets a stat for the current Unit
         * @param stat The stat you want to get, ie: Stats::STAT_STRENGTH
         * @return the value the given stat has
         * \see Stats
         */
        float GetStat(Stats stat) const { return float(GetUInt32Value(UNIT_FIELD_STAT0 + stat)); }
        /**
         * Sets a stat for this Unit
         * @param stat the stat to change
         * @param val the value to change it to
         * \see Stats
         */
        void SetStat(Stats stat, int32 val) { SetStatInt32Value(UNIT_FIELD_STAT0 + stat, val); }
        /**
         * Gets the armor for this Unit
         * @return the current armor
         * \see SpellSchools
         */
        uint32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL) ; }
        /**
         * Sets the armor for this Unit
         * @param val the value to set the armor to
         * \see SpellSchools
         */
        void SetArmor(int32 val) { SetResistance(SPELL_SCHOOL_NORMAL, val); }

        /**
         * Gets the resistance against a certain spell school, ie: fire, frost, nature etc
         * @param school the type of resistance you want to get
         * @return the current resistance against the given school
         */
        uint32 GetResistance(SpellSchools school) const { return GetUInt32Value(UNIT_FIELD_RESISTANCES + school); }
        /**
         * Sets a resistance for this Unit
         * @param school the type of resistance you want to set
         * @param val the value to set it to
         */
        void SetResistance(SpellSchools school, int32 val) { SetStatInt32Value(UNIT_FIELD_RESISTANCES + school, val); }

        /**
         * Gets the health of this Unit
         * @return the current health for this unit
         * \see EUnitFields
         * \see GetUInt32Value
         */
        uint32 GetHealth()    const { return GetUInt32Value(UNIT_FIELD_HEALTH); }
        /**
         * Gets the maximum health of this Unit
         * @return the max health this Unit can have
         * \see EUnitFields
         * \see GetUInt32Value
         */
        bool HealthAbovePctHealed(int32 pct, uint32 heal) const { return uint64(GetHealth()) + uint64(heal) > CountPctFromMaxHealth(pct); }          
        uint32 GetMaxHealth() const { return GetUInt32Value(UNIT_FIELD_MAXHEALTH); }
        bool IsFullHealth() const { return GetHealth() == GetMaxHealth(); }
        bool HealthBelowPct(int32 pct) const { return GetHealth() < CountPctFromMaxHealth(pct); }
        bool HealthBelowPctDamaged(int32 pct, uint32 damage) const { return int64(GetHealth()) - int64(damage) < int64(CountPctFromMaxHealth(pct)); }
        bool HealthAbovePct(int32 pct) const { return GetHealth() > CountPctFromMaxHealth(pct); }
        /**
         * Gets the percent of the health. The formula: (GetHealth() * 100) / GetMaxHealth()
         * @return the current percent of the health
         * \see GetHealth
         * \see GetMaxHealth
         */
        float GetHealthPercent() const { return (GetHealth() * 100.0f) / GetMaxHealth(); }
        uint32 CountPctFromMaxHealth(int32 pct) const { return (GetMaxHealth() * static_cast<float>(pct) / 100.0f); }
        uint32 CountPctFromCurHealth(int32 pct) const { return (GetHealth() * static_cast<float>(pct) / 100.0f); }  
        /**
         * Sets the health to the given value, it cant be higher than Unit::GetMaxHealth though
         * @param val the value to set the health to
         */
        void SetHealth(uint32 val);
        /**
         * Sets the max health for this Unit, also makes sure to update the party with the new
         * value
         * @param val the new max value for the health
         * \see SetHealth
         * \see GetMaxHealth
         */
        void SetMaxHealth(uint32 val);
        /**
         * Sets the health to a certain percentage
         * @param percent the new percent to change it to, ie: 50.0f, not 0.5f for 50%
         */
        void SetHealthPercent(float percent);
        /**
         * Modifies the health by the difference given. If the character had 100 health and we sent in
         * -150 as the amount to decrease it would return -100 as that is how much it decreased since
         * we cant be under 0 health.
         * @param val the difference to apply to the health, ie: -100 would decrease the life by 100
         * @return how much the Unit gained/lost in health.
         */
        int32 ModifyHealth(int32 val);

        /**
         * Gets the power type for this Unit
         * @return The type of power this Unit uses
         */
        Powers GetPowerType() const { return Powers(GetByteValue(UNIT_FIELD_BYTES_0, 3)); }
        void SetPowerType(Powers power);
        uint32 GetPower(Powers power) const { return GetUInt32Value(UNIT_FIELD_POWER1 + power); }
        uint32 GetMaxPower(Powers power) const { return GetUInt32Value(UNIT_FIELD_MAXPOWER1 + power); }
        void SetPower(Powers power, uint32 val);
        void SetMaxPower(Powers power, uint32 val);
        int32 ModifyPower(Powers power, int32 val);
        /**
         * Mods a power by increasing or decreasing it's value
         * @param power which power to mod
         * @param val how much to increase/decrease the given power
         * @param apply whether to apply or remove the mod
         * \see ApplyModUInt32Value
         */
        void ApplyPowerMod(Powers power, uint32 val, bool apply);
        /**
         * Changes the possible max value of the given Powers power.
         * @param power increase max for this power
         * @param val what to add/remove to/from the current max
         * @param apply whether to apply it or remove it
         * \see ApplyModUInt32Value
         */
        void ApplyMaxPowerMod(Powers power, uint32 val, bool apply);

        /**
         * Gets the attack time until next attack for the given weapon type
         * @param att what attack type we want to get attacktime for
         * @return the current attack time, which takes mods of attack speed into account
         * \see Unit::m_modAttackSpeedPct
         * \see EUnitFields
         * \todo Is the time returned in seconds
         */
        uint32 GetAttackTime(WeaponAttackType att) const { return (uint32)(GetFloatValue(UNIT_FIELD_BASEATTACKTIME + att) / m_modAttackSpeedPct[att]); }
        /**
         * Changes the attack time for a certain weapon type.
         * @param att what attack type we want to change the time for
         * @param val what to set it to
         * \see Unit::m_modAttackSpeedPct
         * \see EUnitFields
         */
        void SetAttackTime(WeaponAttackType att, uint32 val) { SetFloatValue(UNIT_FIELD_BASEATTACKTIME + att, val * m_modAttackSpeedPct[att]); }
        /**
         * Applies a percentage change to a given attack type
         * @param att attack type to mod
         * @param val how many percent to add/remove, ie: 90.0f = 90%
         * @param apply whether to add or remove the effect/mod
         * \see ApplyPercentModFloatVar
         * \see ApplyPercentModFloatValue
         */
        void ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply);
        /**
         * Same as ApplyAttackTimePercentMod but for the casting time of spells
         * instead.
         * @param val how many percent to add/remove, ie: 90.0f = 90%
         * @param apply whether to add or remove the effect/mod
         * \see Unit::ApplyAttackTimePercentMod
         */
        void ApplyCastTimePercentMod(float val, bool apply);

        /**
         * Gets the current sheath state, it is whether your main-weapon, ranged-weapon or no
         * weapon is being shown in your hands
         * @return The current sheath state
         */
        SheathState GetSheath() const { return SheathState(GetByteValue(UNIT_FIELD_BYTES_2, 0)); }
        /**
         * Changes the current sheath state.
         * @param sheathed The new weapon or none of them to show
         * \see Unit::GetSheath
         */
        virtual void SetSheath(SheathState sheathed) { SetByteValue(UNIT_FIELD_BYTES_2, 0, sheathed); }

        /**
         * Gets the faction that this unit currently belongs to, also
         * called faction template id it seems. More data probably to
         * be found in the DBC files.
         * @return The faction this unit belongs to
         * \see EUnitFields
         * \todo Does this link correctly to the EUnitFields?
         * \see EUnitFields::UNIT_FIELD_FACTIONTEMPLATE
         * \see FactionTemplateEntry
         * \see FactionEntry
         */
        uint32 getFaction() const { return GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE); }
        /**
         * Changes the faction a unit belongs to.
         * @param faction Faction to change to
         * \see EUnitFields
         * \todo Does this link correctly to the EUnitFields?
         * \see EUnitFields::UNIT_FIELD_FACTIONTEMPLATE
         * \see FactionTemplateEntry
         * \see FactionEntry
         */
        void setFaction(uint32 faction) { SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, faction); }
        FactionTemplateEntry const* getFactionTemplateEntry() const;
        /**
         * Are we hostile towards the given Unit?
         * @param unit the unit we want to check against
         * @return true if the Unit is considered hostile, false otherwise
         */
        bool IsHostileTo(Unit const* unit) const override;
        /**
         * Is this Unit hostile towards players?
         * @return true if the Unit is hostile towards players, false otherwise
         */
        bool IsHostileToPlayers() const;
        /**
         * Is this Unit friendly towards the given Unit?
         * @param unit the Unit to check against
         * @return true if the Unit is considered friendly to us, false otherwise
         */
        bool IsFriendlyTo(Unit const* unit) const override;
        /**
         * Is this Unit neutral to everyone?
         * @return True if considered neutral to everyone, false otherwise.
         */
        bool IsNeutralToAll() const;
        /**
         * Check if this Unit is a guardian of a contested territory, this is
         * useful when we want to know if we should attack all players or only
         * players not belonging to our "side" ally/horde.
         * @return true if this Unit is a guard in a contested area, false otherwise
         */
        bool IsContestedGuard() const
        {
            if (FactionTemplateEntry const* entry = getFactionTemplateEntry())
                { return entry->IsContestedGuardFaction(); }

            return false;
        }
        /**
         * Is PVP enabled?
         * @return true if this Unit is eligible for PVP fighting
         */
        bool IsPvP() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP); }
        /**
         * Put the Unit into our out of PVP
         * @param state true if we want to set PVP on, false otherwise
         */
        void SetPvP(bool state);
        /**
         * Returns the CreatureType for this Unit. For players this most often is
         * CREATURE_TYPE_HUMANOID unless he/she has shapeshifted or something like that.
         * Ie: Bear form probably wouldn't yield the same return value.
         * For creatures though Creature::GetCreatureInfo() is called and the CreatureInfo::type
         * field is used.
         * @return the CreatureType for this Unit
         * \see CreatureType
         */
        uint32 GetCreatureType() const;
        /**
         * Returns a bitmask representation of CreatureType for this Unit.
         * @return A bitmask representation of GetCreatureType()
         */
        uint32 GetCreatureTypeMask() const
        {
            uint32 creatureType = GetCreatureType();
            return (creatureType >= 1) ? (1 << (creatureType - 1)) : 0;
        }

        /**
         * Gets the current stand state for this Unit as described by UnitStandStateType.
         * @return The current stand state
         * \see UnitStandStateType
         * \see MAX_UNIT_STAND_STATE
         */
        uint8 getStandState() const { return GetByteValue(UNIT_FIELD_BYTES_1, 0); }
        /**
         * Is this Unit sitting down in some way?
         * @return true if the Unit is sitting down, false otherwise
         */
        bool IsSitState() const;
        /**
         * Is this Unit just standing normally? This method will return false
         * even if you would consider the state as standing, ie: when the Unit
         * has the state UNIT_STAND_STATE_SLEEP it is considered not standing.
         * @return true if the Unit is standing normally, false otherwise
         */
        bool IsStandState() const;
        /**
         * Change the stand state for this Unit. For possible values check
         * UnitStandStateType.
         * @param state
         * \see UnitStandStateType
         */
        void SetStandState(uint8 state);

        /**
         * Is this Unit mounted?
         * @return true if it's mounted, false otherwise
         * \see EUnitFields
         */
        bool IsMounted() const { return GetMountID(); }
        /**
         * Gets the currently used mount id.
         * @return id of the currently used mount
         */
        uint32 GetMountID() const { return GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID); }
        /**
         * Mounts this Unit by setting the UNIT_FIELD_MOUNTDISPLAYID to the given mount
         * id and setting the bitflag UNIT_FLAG_MOUNT in UNIT_FIELD_FLAGS. If this Unit
         * is a player pets and such are despawned or not depending on the config option
         * CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT.
         * @param mount the id of the mount to mount
         * @param spellId id of the spell used to summon the mount, if 0 is passed in this is treated
         * as a GM command or the Taxi service mounting the Player.
         */
        void Mount(uint32 mount, uint32 spellId = 0);
        /**
         * Unmounts this Unit by sending the SMSG_DISMOUNT to the client if it was a dismount
         * not issued by a GM / the Taxi service. Also changes the UNIT_FIELD_MOUNTDISPLAYID
         * back to 0 and removes the flag UNIT_FLAG_MOUNT from UNIT_FIELD_FLAGS.
         * @param from_aura if this was true the Unit was probably interrupted by a spell
         * or something hitting it forcing a dismount.
         */
        void Unmount(bool from_aura = false);

        /**
         * Returns the maximum skill value the given Unit can have. Ie: the sword skill can
         * be maxed to 300 at level 60. And when you start a level 1 character you maximum
         * skill with swords (given that you know them) is 5. The formula used is:
         * Current Level * 5
         * @param target target to get maximum skill value for, if this is NULL the
         * returned value is for ourselves.
         * @return the maximum skill level you can have at the your current level.
         * \todo Check out the GetLevelForTarget as it seems it's not doing anything constructive with it's arguments.
         */
        uint16 GetMaxSkillValueForLevel(Unit const* target = NULL) const { return (target ? GetLevelForTarget(target) : getLevel()) * 5; }
        /**
         * Deals damage mods to the given victim. If the victim is dead, flying or in evade
         * mode (for creatures) then the damage is absorbed into absorb and no damage
         * is done.
         * @param pVictim
         * @param damage how much damage we want to try to make, will be updated to how
         * much was actually made
         * @param absorb if this is != NULL it will be updated with how much more from
         * before of the damage that was absorbed. ie: absorb += damage not done
         * \todo Does DamageDeal in the AI's do anything?
         * \todo Fix this comment, doesn't really seem correct.
         */
        void DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb);
        /**
         * Generally deals damage to a Unit.
         * @param pVictim victim that will take damage
         * @param damage the damage to make
         * @param cleanDamage melee damage to make
         * @param damagetype the type of damage we'll be doing, ie: DOT, DIRECT_DAMAGE etc.
         * @param damageSchoolMask what school the damage has
         * @param spellProto prototype for the spell that was cast
         * @param durabilityLoss whether this damage should give a durability loss (10%) on death
         * or not
         * @return probably how much damage was actually dealt?
         * \todo Cleanup this function and split into smaller functions for readability
         */
        uint32 DealDamage(Unit* pVictim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool durabilityLoss);
        /**
         * Generally heals a target for addhealth health
         * @param pVictim the victim to heal
         * @param addhealth how much health to add, modified by Unit::ModifyHealth
         * @param spellProto spell prototype for the spell that made this heal
         * @param critical whether or not this was a critical heal (true => crit)
         * @return how much the target actually gained in health
         */
        int32 DealHeal(Unit* pVictim, uint32 addhealth, SpellEntry const* spellProto, bool critical = false);

        /**
         * Calls CallForAllControlledUnits with CONTROLLED_MINIPET and CONTROLLED_GUARDIAN
         * to make them do something if they should when their owner kills someone/thing
         * @param pVictim the target that was killed
         * \see CallForAllControlledUnits
         * \see ControlledUnitMask
         */
        void PetOwnerKilledUnit(Unit* pVictim);

        /**
         * Hard to figure out what this does.
         * \todo Document this.
         * @param pVictim possible victim of the proc
         * @param procAttacker
         * @param procVictim
         * @param procEx
         * @param amount
         * @param attType
         * @param procSpell
         * \see ProcFlagsEx
         */
        void ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker, uint32 procVictim, uint32 procEx, uint32 amount, WeaponAttackType attType = BASE_ATTACK, SpellEntry const* procSpell = NULL);
        /**
         * Same as for Unit::ProcDamageAndSpell
         * @param isVictim whether the target is considered the victim or not
         * @param pTarget
         * @param procFlag
         * @param procExtra
         * @param attType
         * @param procSpell
         * @param damage
         * \see ProcFlagsEx
         */
        void ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell, uint32 damage);

        /**
         * Handles an emote, for example /charge would write something
         * along the lines: "NAME begins to charge" in orange text. This
         * method checks if it's a command or state, a command usually doesn't
         * show anything while a state would show something, ie stand
         * the character up.
         * @param emote_id id of the emote to handle
         * \see EmotesEntry
         * \todo Is this accurate?
         */
        void HandleEmote(uint32 emote_id);                  // auto-select command/state
        /**
         * Sends a packet to the client SMSG_CLIENT with the emote_id given
         * which in turn probably makes the client show some sort of animation
         * for the given emote_id
         * @param emote_id id of the emote to show
         */
        void HandleEmoteCommand(uint32 emote_id);
        /**
         * Just updates the UNIT_NPC_EMOTESTATE field to the given emote_id.
         * @param emote_id the emote to show
         */
        void HandleEmoteState(uint32 emote_id);
        /**
         * Seems to do some damage to pVictim and also does extra attacks if the Unit
         * has any by recursively calling itself up to Unit::m_extraAttacks times with
         * the extra parameter set to true instead of the default false.
         *
         * Also calculates melee damage using Unit::CalculateMeleeDamage, deals damage and
         * such using Unit::DealDamageMods and also procs any spell that might be interesting
         * (TODO: Is that actually what ProcDamageAndSpell does?) using Unit::ProcDamageAndSpell
         *
         * @param pVictim the victim to hit
         * @param attType what hand (main/off) we were using
         * @param extra whether this was called recursively as an extra attack (true) or not (false)
         */
        void AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType = BASE_ATTACK, bool extra = false);

        /**
         * Calculates the chance that a melee attack will miss the given victim.
         * The cap for miss chance is 0-60%, ie: you can't have a higher miss chance
         * than 60% and not lower than 0%.
         * @param pVictim the victim that will be attacked
         * @param attType type of attack
         * @return percentage between 0-60, ie: 57.0f = 57%
         */
        float MeleeMissChanceCalc(const Unit* pVictim, WeaponAttackType attType) const;

        /**
         * Fills the CalcDamageInfo structure with data about how much damage was done, in what way,
         * how much was absorbed etc. Also checks for different procs and inserts these flags into
         * the structure. Also calculates bonus damage with Unit::MeleeDamageBonusDone and the damage
         * with Unit::CalculateDamage
         * @param pVictim the victim that was hit with damage
         * @param damage how much damage to try to do
         * @param damageInfo this is filled with data about what kind of damage that was done
         * @param attackType type of attack, base/off/ranged
         */
        void CalculateMeleeDamage(Unit* pVictim, CalcDamageInfo* damageInfo, WeaponAttackType attackType = BASE_ATTACK);
        /**
         * Deals melee damage, if the attack was parried we reduce the victims time until next hit
         * instead of the weapons normal time by 20 or 60%.
         * Also, if this is a NPC behind a (usually fleeing) player we have a chance to daze the
         * target. Will update the Judgement aura duration too, and check if the victim given from
         * CalcDamageInfo has any shields up and do damage to them in that case.
         * @param damageInfo used to deal the damage
         * @param durabilityLoss whether or not durability loss should happen
         */
        void DealMeleeDamage(CalcDamageInfo* damageInfo, bool durabilityLoss);
        /**
        * Handles all extra attacks set up by a spell
        */
        void HandleProcExtraAttackFor(Unit* victim);
        /**
         * Calculates how much damage a spell should do, it will do some bonus damage according
         * to which SpellNonMeleeDamage::DmgClass it belongs to, ie: SPELL_DAMAGE_CLASS_RANGED
         * or SPELL_DAMAGE_CLASS_MELEE does bonus melee damage while the others make bonus spell
         * damage. Also reduces the damage done based on armor.
         * After returning this function will have filled the SpellNoneMeleeDamage::damage with
         * how much damage was actually done.
         * @param damageInfo info about attacker, target etc
         * @param damage how much damage to try to do
         * @param spellInfo info about the spell, needed by the helper functions
         * @param attackType what we were attacking with
         * \see Unit::IsSpellCrit
         * \see Unit::CalcArmorReducedDamage
         * \see SpellDmgClass
         */
        void CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage, SpellEntry const* spellInfo, WeaponAttackType attackType = BASE_ATTACK);
        /**
         * Deals actual damage based on info given. Does some checking if the spell actually exists
         * and updates the Judgement aura duration if it's there. Then it calls the DealDamage with
         * a SPELL_DIRECT_DAMAGE instead of DIRECT_DAMAGE to indicate that it was caused by a spell
         * (might be more than just that though)
         * @param damageInfo contains info about what kind of damage we will do etc
         * @param durabilityLoss whether or not durability loss should happen
         */
        void DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss);

        /**
         * Calculates the miss chance for a melee spell (a melee spell could be Sinister Strike).
         * Does this by first calculating the hit chance and then "inversing" that value, ie:
         * 100 - hitchance = misschance. The hit chance can be changed by the aura modifiers
         * SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE for ranged and by
         * SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE for melee attacks.
         * You can also resist miss chance with the spell mod SPELLMOD_RESIST_MISS_CHANCE
         * @param pVictim the victim of the attack
         * @param attType main/off hand attack
         * @param skillDiff the skill difference between attackers weapon skill and the victims defense skill, ie: attacker weapon skill - victim defense skill, the lesser this diff is the higher the chance to hit and the lower the chance to miss
         * @param spell what spell was cast
         *
         * @return a value between 0.0f and 60.0f indicating a 0% - 60% miss chance
         * \see Player::ApplySpellMod
         * \see Unit::GetTotalAuraModifier
         * \see Unit::GetSpellModOwner
         */
        float  MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const* spell);
        /**
         * Tells what happened with the spell that was cast, some spells can't miss and they
         * have the attribute SPELL_ATTR_EX3_CANT_MISS. Also, in PvP you can't dodge or parry
         * when the attacker is behind you, but this is possible in PvE.
         *
         * Creatures with the flag CREATURE_FLAG_EXTRA_NO_PARRY can't parry an attack
         * @param pVictim the victim that was hit
         * @param spell the spell that was cast
         * @return Whether or not the spell hit/was resisted/blocked etc. A successfull cast would result in SPELL_MISS_NONE being returned
         * \see SpellEntry::HasAttribute for checking the SPELL_ATTR_EX3_CANT_MISS
         * \see Creature::GetCreatureInfo for the flags_extra
         */
        SpellMissInfo MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell);
        /**
         * This works pretty much like MeleeSpellHitResult but for magic spells instead.
         * For AOE spells there's a \ref Modifier called \ref AuraType::SPELL_AURA_MOD_AOE_AVOIDANCE
         * that reduces the spells hit chance.
         * @param pVictim the victim that was hit
         * @param spell the spell that was cast
         * @return Whether or not the spell was resisted/blocked etc. Seems the only 2
         * possible values are \ref SpellMissInfo::SPELL_MISS_RESIST or
         * \ref SpellMissInfo::SPELL_MISS_NONE
         * \todo Need use unit spell resistance in calculations (Old comment)
         */
        SpellMissInfo MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell);
        /**
         * This combined \ref Unit::MagicSpellHitResult and \ref Unit::MeleeSpellHitResult and also
         * does checks for if the victim is immune or if it is in evade mode etc. If it's a positive
         * spell it can't miss either. Also takes care of reflects via PROC_EX_REFLECT and removes
         * possible charges that could have been present for reflecting spells. Lastly calls one
         * of the earlier mentioned functions depending on the SpellEntry::DmgClass.
         * Calculate spell hit result can be:
         * Every spell can: Evade/Immune/Reflect/Sucesful hit
         * For melee based spells:
         *   Miss
         *   Dodge
         *   Parry
         * For spells
         *   Resist
         * @param pVictim the victim that was hit
         * @param spell the spell that was cast
         * @param canReflect whether or not this spell can be reflected
         * @return Whether or not the spell was resisted/blocked etc.
         */
        SpellMissInfo SpellHitResult(Unit* pVictim, SpellEntry const* spell, bool canReflect = false);

        /**
         * Returns the units dodge chance
         * @return Units dodge chance in percent as value between 0.0f - 100.0f representing 0% - 100%
         */
        float GetUnitDodgeChance()    const;
        /**
         * Returns the units parry chance
         * @return Units parry chance in percent as value between 0.0f - 100.0f representing 0% - 100%
         */
        float GetUnitParryChance()    const;
        /**
         * Returns the units block chance
         * @return Units block chance in percent as value between 0.0f - 100.0f representing 0% - 100%
         */
        float GetUnitBlockChance()    const;
        /**
         * Returns the units critical hit chance against the given target as a value between
         * 0.0f - 100.0f representing 0% - 100%. Aura modifiers named
         * SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE and SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE
         * can increase the critical hit chance. Also the skill level in defense for the target
         * may increase it by formula: (Our max skill - target defense skill) * 0.04
         * @param attackType weapon we will attack with
         * @param pVictim the victim we want to calculate against
         * @return Units critical hit chance in percent as value between 0.0f - 100.0f representing 0% - 100%
         */
        float GetUnitCriticalChance(WeaponAttackType attackType, const Unit* pVictim) const;

        /**
         * Gets how much the shield would block via \ref Unit::m_auraBaseMod and \ref Unit::GetStat
         * for \ref STAT_STRENGTH. Seems to be implemented only in Player.cpp
         * @return Currently equipped shield block value
         */
        virtual uint32 GetShieldBlockValue() const = 0;
        /**
         * The melee skill for the given Unit. For units this is always their maximum possible
         * for their level, ie: current level * 5, for level 60 this would give a skill of 300
         * @param target the target to find the max skill for, if it's NULL we find the level for us
         * @return The max skill level for the given Unit
         */
        uint32 GetUnitMeleeSkill(Unit const* target = NULL) const { return (target ? GetLevelForTarget(target) : getLevel()) * 5; }
        /**
         * Gets the defense skill for the given target, if the target is a Player and this Unit
         * is a Player the maximum skill for that level is used for balancing. If this Unit isn't
         * a Player we fall back to \ref Unit::GetUnitMeleeSkill for the given target.
         * @param target the target we would like to find our skill value against if any
         * @return The skill value for our defense if a target is sent and both are Players, otherwise
         * skill value for the given target.
         * \todo Is the logic for the return correct in here?
         */
        uint32 GetDefenseSkillValue(Unit const* target = NULL) const;
        /**
         * Get's the skill value for the given weapon type. The same idea as for
         * \ref Unit::GetDefenseSkillValue applies, if both target and this are Players
         * we use the max skill instead of actual skill.
         * @param attType the weapon type we want to find the skill for
         * @param target our target if any, if both are Players this will change outcome of skill
         * @return The skill value that the Unit holds in the given hand via attackType
         * \see Item::GetSkill
         * \see SkillType
         */
        uint32 GetWeaponSkillValue(WeaponAttackType attType, Unit const* target = NULL) const;
        /**
         * Returns the proc chance for one weapon, if the \ref BASE_ATTACK is ready then the
         * proc chance for that is returned, otherwise if the \ref OFF_ATTACK is ready and
         * there's a weapon equipped there that chance will be returned, otherwise 0.
         *
         * The formula used for calculation is rather interesting:
         * Mainhand: GetAttackTime(BASE_ATTACK) * 1.8f / 1000.0f
         * Offhand: GetAttackTime(OFF_ATTACK) * 1.6f / 1000.0f
         * @return first the main weapons proc chance, then the off weapons proc chance.
         * \see GetAttackTime
         * \see isAttackReady
         * \todo Add code tags to the formulas
         */
        float GetWeaponProcChance() const;
        /**
         * This returns the proc per minute chance as a percentage.
         * Comment from cpp file:
         * result is chance in percents (probability = Speed_in_sec * (PPM / 60))
         * @param WeaponSpeed the weapon speed, usually gotten with \ref GetAttackTime
         * @param PPM the proc per minute rate
         * @return the chance for a proc in percent (taken from cpp file)
         * \todo What does this actually do? How/Where is it used?
         */
        float GetPPMProcChance(uint32 WeaponSpeed, float PPM) const;

        /**
         * This acts as a wrapper for \ref Unit::RollMeleeOutcomeAgainst with more parameters,
         * these are initialised from the pVictim using
         *  - \ref Unit::MeleeMissChanceCalc
         *  - \ref Unit::GetUnitCriticalChance
         *  - \ref Unit::GetUnitDodgeChance
         *  - \ref Unit::GetUnitBlockChance
         *  - \ref Unit::GetUnitParryChance
         * @param pVictim the victim to target
         * @param attType with what "hand" you want to attack
         * @return what the hit resulted in, miss/hit etc.
         */
        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType) const;
        /**
         * Calculates what off a few possible things that can happen when a victim is attacked
         * with melee weapons. For a list of the things that could happen see \ref MeleeHitOutcome.
         * There's a few formulas involved here, for more info on them check the cpp file. But as
         * usual, if you're behind your victim they can't parry/block and players can't dodge while
         * mobs can.
         * @param pVictim the victim of the attack
         * @param attType the had to attack with
         * @param crit_chance crit chance against victim
         * @param miss_chance miss chance against victim
         * @param dodge_chance victims dodge chance
         * @param parry_chance victims parry chance
         * @param block_chance victims block chance
         * @param SpellCasted whether or not this was because of a spell of autoattack (false => autoattack)
         * @return what the hit resulted in, miss/hit etc
         */
        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance, bool SpellCasted) const;

        /**
         * @return true if this unit is a vendor, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsVendor()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR); }
        /**
         * @return true if this unit is a trainer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTrainer()      const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER); }
        /**
         * @return true if this unit is a QuestGiver, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsQuestGiver()   const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER); }
        /**
         * @return true if this unit is a gossip, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsGossip()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP); }
        /**
         * @return true if this unit is a taxi, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTaxi()         const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_FLIGHTMASTER); }
        /**
         * @return true if this unit is a GuildMaster, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsGuildMaster()  const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER); }
        /**
         * @return true if this unit is a BattleMaster, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsBattleMaster() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BATTLEMASTER); }
        /**
         * @return true if this unit is a banker, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsBanker()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER); }
        /**
         * @return true if this unit is a innkeeper, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsInnkeeper()    const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_INNKEEPER); }
        /**
         * @return true if this unit is a SpiritHealer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritHealer() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER); }
        /**
         * @return true if this unit is a SpiritGuide, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritGuide()  const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITGUIDE); }
        /**
         * @return true if this unit is a TabardDesigner, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTabardDesigner()const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDDESIGNER); }
        /**
         * @return true if this unit is a Auctioneer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool isAuctioner()    const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER); }
        /**
         * @return true if this unit is a armorer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsArmorer()      const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR); }
        /**
         * Returns if this is a service provider or not, a service provider has one of the
         * following flags:
         * - \ref UNIT_NPC_FLAG_VENDOR
         * - \ref UNIT_NPC_FLAG_TRAINER
         * - \ref UNIT_NPC_FLAG_FLIGHTMASTER
         * - \ref UNIT_NPC_FLAG_PETITIONER
         * - \ref UNIT_NPC_FLAG_BATTLEMASTER
         * - \ref UNIT_NPC_FLAG_BANKER
         * - \ref UNIT_NPC_FLAG_INNKEEPER
         * - \ref UNIT_NPC_FLAG_SPIRITHEALER
         * - \ref UNIT_NPC_FLAG_SPIRITGUIDE
         * - \ref UNIT_NPC_FLAG_TABARDDESIGNER
         * - \ref UNIT_NPC_FLAG_AUCTIONEER
         *
         * @return true if this unit is a ServiceProvider, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsServiceProvider() const
        {
            return HasFlag(UNIT_NPC_FLAGS,
                           UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER |
                           UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                           UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                           UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_AUCTIONEER);
        }
        /**
         * Returns if this is a spirit service or not, a spirit service has one of the
         * following flags:
         * - \ref UNIT_NPC_FLAG_SPIRITHEALER
         * - \ref UNIT_NPC_FLAG_SPIRITGUIDE
         * @return true if this unit is a spirit service, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritService() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE); }

        /**
         * Is this unit flying in taxi?
         * @return true if the Unit has the state \ref UNIT_STAT_TAXI_FLIGHT (is flying in taxi), false otherwise
         * \see hasUnitState
         */
        bool IsTaxiFlying()  const { return hasUnitState(UNIT_STAT_TAXI_FLIGHT); }

        /**
         * Checks to see if a creature, whilst moving along a path, has reached a specific waypoint, or near to
         * @param currentPositionX is the creature's current X ordinate in the game world
         * @param currentPositionY is the creature's current Y ordinate in the game world
         * @param currentPositionZ is the creature's current Z ordinate in the game world
         * @param destinationPositionX is the in game ordinate that we wish to check against the creature's current X ordinate (are they the same, or very close?)
         * @param destinationPositionY is the in game ordinate that we wish to check against the creature's current Y ordinate (are they the same, or very close?)
         * @param destinationPositionZ is the in game ordinate that we wish to check against the creature's current Z ordinate (are they the same, or very close?)
         * @param distanceX is the distance from the creature's current X ordinate to the destination X ordinate 
         * @param distanceY is the distance from the creature's current Y ordinate to the destination Y ordinate 
         * @param distanceZ is the distance from the creature's current Z ordinate to the destination Z ordinate 
         * 
         */
        bool IsNearWaypoint(float currentPositionX, float currentPositionY, float currentPositionZ, float destinationPositionX, float destinationPositionY, float destinationPositionZ, float distanceX, float distanceY, float distanceZ);


        /**
         * Is this unit in combat?
         * @return true if the Unit has the flag \ref UNIT_FLAG_IN_COMBAT (is in combat), false otherwise
         * \see EUnitFields
         * \see UnitFlags
         */
        bool IsInCombat()  const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT); }
        /**
         * Sets this \ref Unit into combat, if it already was this has no bigger meaning if the
         * PvP flag hasn't changed since last time it was applied.
         * @param PvP whether this was a PvP combat or not, this is important for how quick the combat flag will wear away and possibly more
         * @param enemy Our target that we are attacking, only does something if the attacked one is a creature it seems
         * \see ClearInCombat
         * \see SetInCombatWith
         */
        void SetInCombatState(bool PvP, Unit* enemy = NULL);
        void SetInDummyCombatState(bool state);
        /**
         * Sets us in combat with the given enemy, this in turn just does a few small checks for if
         * it's a duel or PvP and then calls \ref Unit::SetInCombatState with the correct value for
         * PvP and enemy
         * @param enemy the enemy that we are attacking/engaging
         * \see ClearInCombat
         * \see SetInCombatState
         */
        void SetInCombatWith(Unit* enemy);
        /**
         * Clears the combat flag for this unit using \ref Object::RemoveFlag and clears
         * the Unit state \ref UnitState::UNIT_STAT_ATTACK_PLAYER. This is not where a
         * \ref Player s PvP flags are cleared, that is handled in \ref Player::UpdateContestedPvP
         * \see EUnitFields
         */
        void ClearInCombat();
        /**
         * Probably returns how long it is until this Unit should get out of PvP combat again,
         * although not used in that sense.
         * @return the time we have left from our start of PvP combat
         * \todo Find out what this actually does/means
         */
        uint32 GetCombatTimer() const { return m_CombatTimer; }

        /**
         * Gets all \ref SpellAuraHolder s that have the same given spell_id
         * @param spell_id the spell_id to search for
         * @return 2 iterators to the range of \ref SpellAuraHolder s that were found
         */
        SpellAuraHolderBounds GetSpellAuraHolderBounds(uint32 spell_id)
        {
            return m_spellAuraHolders.equal_range(spell_id);
        }
        /**
         * Same as \ref Unit::GetSpellAuraHolderBounds
         */
        SpellAuraHolderConstBounds GetSpellAuraHolderBounds(uint32 spell_id) const
        {
            return m_spellAuraHolders.equal_range(spell_id);
        }

        /**
         * Checks if this \ref Unit has the given AuraType
         * @param auraType the type of aura to look for
         * @return true if this \ref Unit is affected by the given \ref AuraType, false otherwise
         */
        bool HasAuraType(AuraType auraType) const;
        /**
         * Checks if the given \ref SpellEntry affects the \ref AuraType in some way, this is done
         * by calling \ref Aura::isAffectedOnSpell which in turn seems to check if the spellProto
         * and the \ref Aura s own \ref SpellEntry have the same \ref SpellEntry::SpellFamilyName.
         * (The \ref Aura get's it's \ref SpellEntry by calling \ref Aura::GetSpellProto)
         * @param auraType the type of aura we want to check
         * @param spellProto the spell we want to check for affecting the aura type
         * @return true if the spell affects the given aura type, false otherwise
         * \todo Is this actually correct, also, make it more clear what it actually checks
         */
        bool HasAffectedAura(AuraType auraType, SpellEntry const* spellProto) const;
        /**
         * Checks if we have at least one \ref Aura that is associated with the given spell id
         * and \ref SpellEffectIndex.
         * @param spellId the spell id to look for
         * @param effIndex the effect index the \ref Aura should have
         * @return true if there was at least one \ref Aura associated with the id that belonged to
         * the correct \ref SpellEffectIndex, false otherwise
         */
        bool HasAura(uint32 spellId, SpellEffectIndex effIndex) const;
        /**
         * Checks if we have at least one \ref Aura that is associated with the given spell id via
         * the \ref Unit::m_spellAuraHolders multimap. Generalized version of the other
         * \ref Unit::HasAura
         * @param spellId the spell id to look for
         * @return true if there was at least one \ref Aura associated with the id, false otherwise
         */
        bool HasAura(uint32 spellId) const
        {
            return m_spellAuraHolders.find(spellId) != m_spellAuraHolders.end();
        }

        /**
         * This is overridden in \ref Player::HasSpell, \ref Creature::HasSpell and \ref Pet::HasSpell
         * @return false in this implementation
         */
        virtual bool HasSpell(uint32 /*spellID*/) const { return false; }

        /**
         * Check is this \ref Unit has a stealth modified applied
         * @return true if this \ref Unit has the \ref AuraType \ref AuraType::SPELL_AURA_MOD_STEALTH
         * applied, false otherwise
         * \see Modifier
         * \see Unit::HasAuraType
         * \see AuraType
         */
        bool HasStealthAura()      const { return HasAuraType(SPELL_AURA_MOD_STEALTH); }
        /**
         * Check if this \ref Unit has a invisibility \ref Aura modifier applied.
         * @return true if this \ref Unit has the \ref AuraType
         * \ref AuraType::SPELL_AURA_MOD_INVISIBILITY applied, false otherwise
         * \see Modifier
         * \see Unit::HasAuraType
         * \see AuraType
         */
        bool HasInvisibilityAura() const { return HasAuraType(SPELL_AURA_MOD_INVISIBILITY); }
        /**
         * Check if this \ref Unit has a fear \ref Aura modifier applied. Ie, is it feared?
         * @return true if this \ref Unit has the \ref AuraType \ref AuraType::SPELL_AURA_MOD_FEAR
         * applied, false otherwise
         * \see Modifier
         * \see Unit::HasAuraType
         * \see AuraType
         */
        bool IsFeared()  const { return HasAuraType(SPELL_AURA_MOD_FEAR); }
        /**
         * Check if this \ref Unit has a rooting \ref Aura modifier applied. Ie, is it stuck in
         * some way?
         * @return true if this \ref Unit has the \ref AuraType \ref AuraType::SPELL_AURA_MOD_ROOT
         * applied, false otherwise
         * \see Modifier
         * \see Unit::HasAuraType
         * \see AuraType
         */
        bool IsInRoots() const { return HasAuraType(SPELL_AURA_MOD_ROOT); }
        /**
         * Is this \ref Unit polymorphed?
         * @return true if this \ref Unit is polymorphed, false otherwise
         * \see GetSpellSpecific
         * \see Unit::GetTransform
         * \todo Move the implementation to .h file exactly as the earlier ones?
         */
        bool IsPolymorphed() const;

        /**
         * Check if this \ref Unit has  freezing \ref Aura modifier applied. Ie, is it
         * frozen in ground?
         * @return true if this \ref Unit has the \ref AuraType \ref AuraType::AURA_STATE_FROZEN,
         * false otherwise
         * \see Modifier
         * \see Unit::HasAuraType
         * \see AuraType
         * \todo Move the implementation to .h file exactly as the earlier ones?
         */
        bool IsFrozen() const;

        /**
         * Checks if this \ref Unit could be targeted with an attack, things that make that
         * impossible are:
         * - The \ref Unit / \ref Player is a GM
         * - The \ref Unit has the flags (\ref Object::HasFlag)
         * \ref UnitFlags::UNIT_FLAG_NON_ATTACKABLE
         * and \ref UnitFlags::UNIT_FLAG_NOT_SELECTABLE
         * - The \ref Unit has the flag (\ref Object::HasFlag)
         * \ref UnitFlags::UNIT_FLAG_OOC_NOT_ATTACKABLE, this seems to vary some though, since this
         * flag will be removed when the creature for some reason enters combat
         * - \ref Unit::IsAlive is equal to inverseAlive
         * - \ref Unit::IsInWorld is false
         * - the \ref Unit has the state (\ref Unit::hasUnitState) \ref  UnitState::UNIT_STAT_DIED
         * - the \ref Unit is flying in a taxi (\ref Unit::IsTaxiFlying)
         * @param inverseAlive This is needed for some spells which need
         * to be casted at dead targets (aoe) (Taken from source comment)
         * @return true if the target can be attacked, false otherwise
         * \see UnitState
         */
        bool IsTargetableForAttack(bool inverseAlive = false) const;
        /**
         * Simply checks if this \ref Unit has the flag (\ref Unit::HasFlag)
         * \ref UnitFlags::UNIT_FLAG_PASSIVE in \ref EUnitFields::UNIT_FIELD_FLAGS
         * @return true if the target is passive to hostile actions, false otherwise
         */
        bool isPassiveToHostile() { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE); }

        /**
         * Is this \ref Unit in water?
         * @return true if the \ref Unit is in water, false otherwise
         * \see Object::GetTerrain
         * \see TerrainInfo::IsInWater
         */
        virtual bool IsInWater() const;
        /**
         * Is this \ref Unit under water?
         * @return true if the \ref Unit is under water, false otherwise
         * \see Object::GetTerrain
         * \see TerrainInfo::IsUnderWater
         */
        virtual bool IsUnderWater() const;
        /**
         * Can the given \ref Creature access this \ref Unit in some way? If this \ref Unit is in
         * water we check if the \ref Creature can swim, if so it's accessible, otherwise it's not.
         * If we're not in water the \ref Creature should be able to walk or fly and then we're
         * accessible.
         * @param c The \ref Creature to check accessibility for
         * @return true if this \ref Unit is accessible to the \ref Creature given, false otherwise
         * \todo Rename to IsInAccessablePlaceFor to follow standards?
         */
        bool isInAccessablePlaceFor(Creature const* c) const;

        /**
         * Sends a packet to the client with \ref OpcodesList::SMSG_SPELLHEALLOG which presumably
         * updates the combat log of this \ref Unit and shows some healing done and to who in it.
         *
         * For a description of the packets look see \ref OpcodesList::SMSG_SPELLHEALLOG
         * @param pVictim the victim of the healing spell
         * @param SpellID what spell id that was used
         * @param Damage how much "damage" we did (healing in this case)
         * @param critical whether it was a critical hit or not.
         * \see WorldPacket
         */
        void SendHealSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical = false);
        /**
         * Sends a packet to the client with \ref OpcodesList::SMSG_SPELLENERGIZELOG which presumably
         * updates the combat log of this \ref Unit and shows some enery regen and to who it was.
         *
         * For a description of the packets look see \ref OpcodesList::SMSG_SPELLENERGIZELOG
         * @param pVictim the victim of the regen
         * @param SpellID the spell id that caused it
         * @param Damage how much was regenerated
         * @param powertype the power that was regenerated
         */
        void SendEnergizeSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);
        /**
         * Regenerates/degenerates the given amount of "damage" for the given power and sends a
         * update to the combat log.
         * @param pVictim the victim to add/remove power from
         * @param SpellID the spell id that caused it
         * @param Damage how much was increased/decreased, negative values decrease, positive increase
         * @param powertype the power that was increased/decreased
         * \see Unit::ModifyPower
         */
        void EnergizeBySpell(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);
        /**
         * Will do damage to the victim calculating how much should be done with the help of
         * \ref SpellNonMeleeDamage, \ref Unit::CalculateSpellDamage, \ref Unit::DealDamageMods and
         * \ref Unit::SendSpellNonMeleeDamageLog to update the combat log
         * @param pVictim the victim that should take damage
         * @param spellID id of the spell that will cause the damage
         * @param damage the initial damage to do
         * @return how much damage was actually done
         */
        uint32 SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage);
        /**
         * This function only checks if the spell id is accurate, if it is then the other
         * \ref Unit::CastSpell is called which does the actual cast.
         * @param Victim victim that should be hit by the spell
         * @param spellId id of the spell to cast
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on you action bar, true means triggered by outside circumstance
         * @param castItem the item that cast the spell if any, usually NULL
         * @param triggeredByAura the \ref Aura that triggered the spell if any
         * @param originalCaster usually just \ref ObjectGuid constructor
         * @param triggeredBy the \ref SpellEntry that triggered this spell if any
         * \see Spell
         * \see SpellCastTargets
         * \todo What's the original caster?
         */
        void CastSpell(Unit* Victim, uint32 spellId, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);
        /**
         * Casts a spell simple and square, outputs some debugging info for some reasons, ie: if the
         * spellInfo is NULL it is logged and the function won't do anything. If the spell is triggered
         * by an \ref Aura and there's no originalCaster it is updated to be the cast of the \ref Aura
         * and the triggeredBy is set to be the \ref Aura s \ref SpellEntry. Some work is done to
         * work out if we should set a destination and/or source for the spell and it is then cast.
         *
         * Also, linked spells seem to be taken care of in here, but only the ones that should be
         * removed on cast, ie: \ref SpellLinkedType::SPELL_LINKED_TYPE_REMOVEONCAST. See
         * \ref SpellLinkedEntry for more info and \ref SpellLinkedType
         *
         * Finally calls \ref Spell::prepare on the \ref Spell and that's where it continues the
         * execution.
         *
         * @param Victim victim that should be hit by the spell
         * @param spellInfo info about the spell to cast
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on your action bar, true means triggered by outside circumstance
         * @param castItem the \ref Item that cast the spell if any, usually NULL
         * @param triggeredByAura the \ref Aura that triggered the spell if any
         * @param originalCaster usually just \ref ObjectGuid constructor
         * @param triggeredBy the \ref SpellEntry that triggered this spell if any
         * \see Spell
         * \see SpellCastTargets
         * \todo What's the original caster?
         * \todo Document the spell linked
         */
        void CastSpell(Unit* Victim, SpellEntry const* spellInfo, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);
        /**
         * Does pretty much the same thing as \ref Unit::CastSpell but uses the three bp0-bp2 variables
         * to change the \ref Spell s \ref Spell::m_currentBasePoints for the different
         * \ref SpellEffectIndexes. This also works as the first version of \ref Unit::CastSpell which
         * just does some checks and then calls the other one.
         *
         * @param Victim victim that should be hit by the spell
         * @param spellId id of the spell to be cast
         * @param bp0 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_0 to this value if it's not NULL.
         * @param bp1 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_1 to this value if it's not NULL.
         * @param bp2 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_2 to this value if it's not NULL.
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on your action bar, true means triggered by outside circumstance
         * @param castItem the \ref Item that cast this if any
         * @param triggeredByAura the \ref Aura that triggered this
         * @param originalCaster the original caster if any
         * @param triggeredBy the \ref SpellEntry that triggered this cast, if any
         * \todo What's the original caster?
         */
        void CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);
        /**
         * Same idea for this one as for \ref Unit::CastCustomSpell with a change to the spellid being
         * exchanged for a \ref SpellEntry instead
         *
         * @param Victim victim that should be hit by the spell
         * @param spellInfo info about the spell to cast
         * @param bp0 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_0 to this value if it's not NULL.
         * @param bp1 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_1 to this value if it's not NULL.
         * @param bp2 this will change the \ref Spell s member \ref Spell::m_currentBasePoints for
         * \ref SpellEffectIndex::EFFECT_INDEX_2 to this value if it's not NULL.
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on your action bar, true means triggered by outside circumstance
         * @param castItem the \ref Item that cast this if any
         * @param triggeredByAura the \ref Aura that triggered this
         * @param originalCaster the original caster if any
         * @param triggeredBy the \ref SpellEntry that triggered this cast, if any
         * @param calculateDamage Indicates whether the damage calculation must be performed (in some cases, the calculation has already been executed).
         * \todo What's the original caster?
         */
        void CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);
        /**
         * Same idea as for \ref Unit::CastSpell, but with the parameters x, y, z telling the
         * destination and source of the \ref SpellCastTargets depending on if the
         * \ref SpellEntry::Targets bitflags have the
         * \ref SpellCastTargetFlags::TARGET_FLAG_DEST_LOCATION  set for destination and
         * \ref SpellCastTargetFlags::TARGET_FLAG_SOURCE_LOCATION set for location
         *
         * @param x coord for source/dest
         * @param y coord for source/dest
         * @param z coord for source/dest
         * @param spellId id of the spell that was cast
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on your action bar, true means triggered by outside circumstance
         * @param castItem the \ref Item that cast this if any
         * @param triggeredByAura the \ref Aura that triggered this
         * @param originalCaster the original caster if any
         * @param triggeredBy the \ref SpellEntry that triggered this cast, if any
         */
        void CastSpell(float x, float y, float z, uint32 spellId, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);
        /**
         * Same idea as for \ref Unit::CastSpell, but with the parameters x, y, z telling the
         * destination and source of the \ref SpellCastTargets depending on if the
         * \ref SpellEntry::Targets bitflags have the
         * \ref SpellCastTargetFlags::TARGET_FLAG_DEST_LOCATION  set for destination and
         * \ref SpellCastTargetFlags::TARGET_FLAG_SOURCE_LOCATION set for location
         *
         * @param x coord for source/dest
         * @param y coord for source/dest
         * @param z coord for source/dest
         * @param spellInfo info about the spell to cast
         * @param triggered whether this was triggered by some outside circumstance or used as a button
         * press on your action bar, true means triggered by outside circumstance
         * @param castItem the \ref Item that cast this if any
         * @param triggeredByAura the \ref Aura that triggered this
         * @param originalCaster the original caster if any
         * @param triggeredBy the \ref SpellEntry that triggered this cast, if any
         */
        void CastSpell(float x, float y, float z, SpellEntry const* spellInfo, bool triggered, Item* castItem = NULL, Aura* triggeredByAura = NULL, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = NULL);

        /**
         * Changes the display id for this \ref Unit to the native one that it usually has.
         * This is done by calling \ref Unit::SetDisplayId and \ref Unit::GetNativeDisplayId
         * like so:
         * \code{.cpp}
         * SetDisplayId(GetNativeDisplayId());
         * \endcode
         */
        void DeMorph();

        /**
         * This sends an AttackStateUpdate, some info about damage that you've done etc.
         * @param damageInfo the damage info used for knowing what to send
         * \see OpcodesList::SMSG_ATTACKERSTATEUPDATE
         * \todo Find out when and why this is sent
         */
        void SendAttackStateUpdate(CalcDamageInfo* damageInfo);
        /**
         * The same thing as \ref Unit::SendAttackStateUpdate but you send along all the parameters
         * that are needed instead of giving them through \ref CalcDamageInfo
         * @param HitInfo hit information as in the \ref CalcDamageInfo::HitInfo
         * @param target the target of the attack
         * @param damageSchoolMask the damageschoolmask as the one from:
         * \ref CalcDamageInfo::damageSchoolMask
         * @param Damage the damage that was done
         * @param AbsorbDamage how much of the damage that was absorbed
         * @param Resist how much of the damage that was resisted
         * @param TargetState the \ref VictimState of the target
         * @param BlockedAmount how much of the damage that was blocked
         * \todo What's the swingtype for?
         */
        void SendAttackStateUpdate(uint32 HitInfo, Unit* target, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, uint32 Resist, VictimState TargetState, uint32 BlockedAmount);
        /**
         * Used to send a update to the combat log for all \ref Player/\ref Unit s in the vicinity.
         * @param log Info about who/what did damage to who and how etc, data needed for the packet
         * \see OpcodesList::SMSG_SPELLNONMELEEDAMAGELOG
         * \todo Is this actually for the combat log?
         */
        void SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log);
        /**
         * Same idea as for \ref Unit::SendSpellNonMeleeDamageLog but without the helping
         * \ref SpellNonMeleeDamage. This will set the \ref SpellNonMeleeDamage::HitInfo member to
         * the following before sending the packet:
         * \code {.cpp}
         * \ref HitInfo::SPELL_HIT_TYPE_UNK1 | \ref HitInfo::SPELL_HIT_TYPE_UNK3 | \ref HitInfo::SPELL_HIT_TYPE_UNK6
         * \endcode
         *
         * And if the \a CriticalHit parameter is true then it will add the flag
         * \ref HitInfo::SPELL_HIT_TYPE_CRIT
         *
         * @param target the target of the spell
         * @param SpellID id of the spell that was used
         * @param Damage the damage done including the damage that was resisted/absorbed/blocked etc.
         * Ie: damage + absorbed + resisted. This will be subtracted to the bare damage when inserted
         * into the \ref SpellNonMeleeDamage struct
         * @param damageSchoolMask mask for which kind of damage this is, see \ref SpellSchools for
         * possible values, the first set bit out of this mask will be used a the \ref SpellSchools
         * @param AbsorbedDamage how much of the damage that was absorbed
         * @param Resist how much of the damage that was resisted
         * @param PhysicalDamage whether or not this was physical damage
         * @param Blocked how much of the damage that was blocked
         * @param CriticalHit whether it was a critical hit or not
         * \see HitInfo
         * \see OpcodesList::SMSG_SPELLNONMELEEDAMAGELOG
         * \todo Is this actually for the combat log?
         */
        void SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID, uint32 Damage, SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage, uint32 Resist, bool PhysicalDamage, uint32 Blocked, bool CriticalHit = false);
        /**
         * Sends some data to the combat log about the periodic effects of an \ref Aura, it might be
         * periodic healing/damage etc. Perhaps it increases the amount of power you have as a rogue
         * and such. For more info on what exactly is sent etc see
         * \ref OpcodesList::SMSG_PERIODICAURALOG.
         * @param pInfo Info about the periodic effect of the \ref Aura
         * \see OpcodesList::SMSG_PERIODICAURALOG
         * \todo Is this actually for the combat log?
         */
        void SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo);
        /**
         * Sends some data to the combat log about a spell that missed someone else. For more info
         * on what's sent see \ref OpcodesList::SMSG_SPELLLOGMISS
         * @param target the target of the \ref Spell that missed
         * @param spellID id of the spell that missed
         * @param missInfo info about how the spell actually missed ie: resisted/blocked/reflected etc.
         */
        void SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo);

        /**
         * Teleports a \ref Creature or \ref Player to some coordinates within the same \ref Map,
         * hence the name. If it's a \ref Creature that's being teleported it needs to have it's
         * \ref MovementGenerator interrupted before the teleport and then reset afterwards. See
         * \ref MovementGenerator::Reset and \ref MovementGenerator::Interrupt. Also, after moving
         * a \ref Creature a hearbeat needs to be sent to inform the clients about the new location,
         * this is done using \ref Unit::SendHeartBeat and the actual move of the \ref Creature is
         * done with \ref Map::CreatureRelocation
         * @param x the new x coord
         * @param y the new y coord
         * @param z the new z coord
         * @param orientation the orientation after teleport, 0 is north and it's measured in radians
         * @param casting used to decide whether a spell cast should be interrupted if a \ref Player
         * is going to be teleported, see \ref TeleportToOptions::TELE_TO_SPELL, the idea is that
         * you don't interrupt spellcasting at teleport if the spell is meant to teleport you as
         * that would defeat the purpose of trying to teleport one self :)
         */
        void NearTeleportTo(float x, float y, float z, float orientation, bool casting = false);
        /**
         * Moves this \ref Unit to the given position in x,y,z coordinates. we can choose whether or
         * not we want to generate a path to the target or just move straight there and if we would
         * like to force the destination when creating the \ref PathFinder, only interesting
         * if we have set generatePath to true.
         *
         * Taken from comments: recommend use \ref Unit::MonsterMove / \ref Unit::MonsterMoveWithSpeed
         * for most case that correctly work with movegens, mmaps
         * @param x the x coord to move to
         * @param y the y coord to move to
         * @param z the z coord to move to
         * @param speed at which speed the \ref Unit should move
         * @param generatePath whether a real path should be generated using a \ref PathFinder which
         * will try to create a smooth path. if false we just go from current pos to given pos
         * @param forceDestination if this is true it seems that we will try to get to our target
         * even if there's something in the way. Otherwise we stop before we get there if there
         * are obstacles
         * \todo In what is the speed expressed? What is normal walking/running speed?
         * \todo Is the dox about forceDestination correct?
         */
        void MonsterMoveWithSpeed(float x, float y, float z, float speed, bool generatePath = false, bool forceDestination = false);

        // if used additional args in ... part then floats must explicitly casted to double
        /**
         * Tells nearby \ref Unit s and such that this \ref Unit has moved to a new position using
         * \ref OpcodesList::MSG_MOVE_HEARTBEAT which will send the new position to all clients etc
         * in the same \ref Cell
         */
        void SendHeartBeat();

        /**
         * Checks if this \ref Unit has the movement flag \ref MovementFlags::MOVEFLAG_LEVITATING
         * @return true if the \ref Unit is levitating, ie: it has the flag MOVEFLAG_LEVITATING, false
         * otherwise
         * \see MovementInfo::HasMovementFlag
         */
        bool IsLevitating() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING); }
        /**
         * Checks if this \ref Unit has the movement flag \ref MovementFlags::MOVEFLAG_WALK_MODE
         * @return true if the \ref Unit is walking, ie: it has the flag MOVEFLAG_WALK_MODE, false
         * otherwise
         * \see MovementInfo::HasMovementFlag
         */
        bool IsWalking() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE); }
        /**
         * Check if this \ref Unit has the movement flag \ref MovementFlags::MOVEFLAG_ROOT
         * @return true if the \ref Unit is rooted to the ground (can't move), ie: has the flag
         * MOVEFLAG_ROOT, false otherwise
         * \see MovementInfo::HasMovementFlag
         */
        bool IsRooted() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_ROOT); }
        /**
         * Roots or unroots this \ref Unit depending on the enabled parameter.
         * @param enabled whether we should root (true) or unroot (false) this \ref Unit
         * \see Player::SetRoot
         */
        virtual void SetLevitate(bool /*enabled*/) {}
        virtual void SetSwim(bool /*enabled*/) {}
        virtual void SetCanFly(bool /*enabled*/) {}
        virtual void SetFeatherFall(bool /*enabled*/) {}
        virtual void SetHover(bool /*enabled*/) {}
        virtual void SetRoot(bool /*enabled*/) {}
        /**
         * Changes this \ref Unit s ability to walk on water.
         * @param enabled whether this \ref Unit should be able to walk on water (true) or not
         * be able to (false)
         * \see Player::SetWaterWalk
         */
        virtual void SetWaterWalk(bool /*enabled*/) {}

        /**
         * Turns this \ref Unit towards the given one.
         * @param target the \ref Unit we want to be turned towards
         * \see WorldObject::SetOrientation
         * \see WorldObejct::GetAngle
         */
        void SetInFront(Unit const* target);
        /**
         * Sets this \ref Unit to face a certain angle.
         * @param ori where we should start facing, measured in radians, 0 = north pi/2 = east etc.
         * \todo is pi/2 = east or west? Logic says east?
         */
        void SetFacingTo(float ori);
        /**
         * Does pretty much the same thing as \ref Unit::SetInFront but calls \ref Unit::SetFacingTo
         * instead, which uses a \ref MoveSplineInit instead of just changing the angle.
         * @param pObject the \ref WorldObject we should be facing
         * \todo What difference does it make to use \ref MoveSplineInit instead of just directly
         * changing the angle? Is it smoother?
         */
        void SetFacingToObject(WorldObject* pObject);

        /**
         * Checks whether or not this \ref Unit is alive by checking the \ref Unit::m_deathState member
         * for the value \ref DeathState::ALIVE
         * @return true if this \ref Unit is alive, false otherwise
         */
        bool IsAlive() const { return (m_deathState == ALIVE); };
        bool IsDying() const { return (m_deathState == JUST_DIED); }
        /**
         * Checks whether or not this \ref Unit is dead by checking the \ref Unit::m_deathState member
         * for the value \ref DeathState::DEAD or \ref DeathState::CORPSE
         * @return true if this \ref Unit is dead or a corpse (also dead), false otherwise
         */
        bool IsDead() const { return (m_deathState == DEAD || m_deathState == CORPSE); };
        /**
         * Returns the current \ref DeathState for this \ref Unit.
         * @return the value of the member \ref Unit::m_deathState
         */
        DeathState GetDeathState() const { return m_deathState; };
        /**
         * Changes the \ref DeathState for this \ref Unit and making sure that some things that should
         * happen when that changes happen, ie: you just died, then you're auras should be removed,
         * any combopoints that you had should be removed etc.
         *
         * This is overwritten to do different things in at least \ref Player, \ref Creature, \ref Pet
         * @param s the new \ref DeathState this \ref Unit should get
         */
        virtual void SetDeathState(DeathState s);           // overwritten in Creature/Player/Pet

        ObjectGuid const& GetOwnerGuid() const { return  GetGuidValue(UNIT_FIELD_SUMMONEDBY); }
        void SetOwnerGuid(ObjectGuid owner) { SetGuidValue(UNIT_FIELD_SUMMONEDBY, owner); }
        ObjectGuid const& GetCreatorGuid() const { return GetGuidValue(UNIT_FIELD_CREATEDBY); }
        void SetCreatorGuid(ObjectGuid creator) { SetGuidValue(UNIT_FIELD_CREATEDBY, creator); }
        ObjectGuid const& GetPetGuid() const { return GetGuidValue(UNIT_FIELD_SUMMON); }
        void SetPetGuid(ObjectGuid pet) { SetGuidValue(UNIT_FIELD_SUMMON, pet); }
        ObjectGuid const& GetCharmerGuid() const { return GetGuidValue(UNIT_FIELD_CHARMEDBY); }
        void SetCharmerGuid(ObjectGuid owner) { SetGuidValue(UNIT_FIELD_CHARMEDBY, owner); }
        ObjectGuid const& GetCharmGuid() const { return GetGuidValue(UNIT_FIELD_CHARM); }
        void SetCharmGuid(ObjectGuid charm) { SetGuidValue(UNIT_FIELD_CHARM, charm); }
        ObjectGuid const& GetTargetGuid() const { return GetGuidValue(UNIT_FIELD_TARGET); }
        void SetTargetGuid(ObjectGuid targetGuid) { SetGuidValue(UNIT_FIELD_TARGET, targetGuid); }
        ObjectGuid const& GetChannelObjectGuid() const { return GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT); }
        void SetChannelObjectGuid(ObjectGuid targetGuid) { SetGuidValue(UNIT_FIELD_CHANNEL_OBJECT, targetGuid); }

        /**
         * Returns the currently spawned minipet, this has an implementation in \ref Player
         * @return the current \ref Pet for this \ref Player
         */
        virtual Pet* GetMiniPet() const { return NULL; }    // overwrited in Player

        /** 
         * Gets either the current charmer (ie mind control) or the owner of this \ref Unit
         * @return the \ref ObjectGuid of either the charmer of this \ref Unit or the owner of it
         */
        ObjectGuid const& GetCharmerOrOwnerGuid() const { return GetCharmerGuid() ? GetCharmerGuid() : GetOwnerGuid(); }
        /** 
         * Same thing as \ref Unit::GetCharmerOrOwnerGuid but with the exception that it returns
         * it's own \ref ObjectGuid if it has no owner or charmer.
         * @return either the charmers, owners or it's own \ref ObjectGuid
         */
        ObjectGuid const& GetCharmerOrOwnerOrOwnGuid() const
        {
            if (ObjectGuid const& guid = GetCharmerOrOwnerGuid())
                { return guid; }
            return GetObjectGuid();
        }
        /** 
         * Checks if the charmer or owner is a \ref Player
         * @return true if the charmer or owner is a \ref Player, false otherwise
         * \see ObjectGuid::IsPlayer
         */
        bool IsCharmedOwnedByPlayerOrPlayer() const { return GetCharmerOrOwnerOrOwnGuid().IsPlayer(); }

        /** 
         * Get's the \ref Player that owns the \ref SpellModifier for this \ref Unit, if this
         * \ref Unit is a \ref Player it's the owner, but if it's a \ref Pet och \ref Totem then
         * then owner of the totem is returned if it's a \ref Player
         * @return The \ref SpellModifier owner for this \ref Unit
         */
        Player* GetSpellModOwner() const;

        /** 
         * Returns the \ref Unit that owns this \ref Unit if any
         * @return the \ref Unit that owns this one, NULL if there is no owner
         * \see Unit::GetOwnerGuid
         */
        Unit* GetOwner() const;
        /** 
         * Returns the \ref Pet for this \ref Unit if any
         * @return the \ref Pet that is associated with this \ref Unit if any, NULL if there is none
         * \see Unit::GetPetGuid
         */
        Pet* GetPet() const;
        /** 
         * Returns the \ref Unit that's currently charming this one if any.
         * @return the \ref Unit that's charming this one, NULL if there is none
         */
        Unit* GetCharmer() const;
        /** 
         * Returns the \ref Unit that this one is currently charming
         * @return the \ref Unit that this one is charming, NULL if there is none
         */
        Unit* GetCharm() const;
        /** 
         * Removes all \ref Aura s causing this \ref Unit to be charmed/possessed, the \ref Aura s
         * that cause this are:
         * - \ref AuraType::SPELL_AURA_MOD_CHARM
         * - \ref AuraType::SPELL_AURA_MOD_POSSESS
         * - \ref AuraType::SPELL_AURA_MOD_POSSESS_PET
         */
        void Uncharm();
        /** 
         * Does the same as \ref Unit::GetCharmerOrOwnerGuid but returns the \ref Unit for that instead
         * @return the \ref Unit that's charming this one or owning it, NULL if there is none
         */
        Unit* GetCharmerOrOwner() const { return GetCharmerGuid() ? GetCharmer() : GetOwner(); }
        /** 
         * Does the same a \ref Unit::GetCharmerOrOwner but if there is none of those it returns itself
         * @return a \ref Unit that's either owning or charming this one or just itself.
         */
        Unit* GetCharmerOrOwnerOrSelf()
        {
            if (Unit* u = GetCharmerOrOwner())
                { return u; }

            return this;
        }
        bool IsCharmerOrOwnerPlayerOrPlayerItself() const;
        Player* GetCharmerOrOwnerPlayerOrPlayerItself();
        Player const* GetCharmerOrOwnerPlayerOrPlayerItself() const;

        /** 
         * Set's the current \ref Pet for this \ref Unit
         * @param pet The \ref Pet to add to this \ref Unit
         */
        void SetPet(Pet* pet);
        /** 
         * Set's who we're currently charming
         * @param pet The \ref Unit to set as charmed by us
         */
        void SetCharm(Unit* pet);

        /** 
         * Adds a guardian to this \ref Unit which will generally defend this \ref Unit when on a
         * threat list.
         * @param pet the guardian to add
         * \see Unit::m_guardianPets
         */
        void AddGuardian(Pet* pet);
        /** 
         * Removes a guardian from this \ref Unit
         * @param pet the guardian to remove
         * \see Unit::m_guardianPets
         */
        void RemoveGuardian(Pet* pet);
        /** 
         * Removes all current guardians from this \ref Unit
         */
        void RemoveGuardians();
        /** 
         * Finds a guardian by it's entry, this is the entry in character.character_pet
         * @param entry the entry to find
         * @return the guardian/\ref Pet found or NULL if there's no such entry in the db
         * \todo Is it the correct entry
         */
        Pet* FindGuardianWithEntry(uint32 entry);

        /** 
         * Is this \ref Unit charmed?
         * @return true if the \ref Unit has a charmer, false otherwise
         * \see Unit::GetCharmerGuid
         */
        bool IsCharmed() const { return !GetCharmerGuid().IsEmpty(); }

        /** 
         * There's only \ref CharmInfo available if this \ref Unit is in fact charmed by someone
         * @return The \ref CharmInfo for this \ref Unit if any, NULL otherwise
         */
        CharmInfo* GetCharmInfo() { return m_charmInfo; }
        /** 
         * Init the \ref CharmInfo struct with data about the \ref Unit that will be charmed
         * @param charm the \ref Unit that is to be charmed
         * @return the created \ref CharmInfo
         * \todo Is the charm param really the unit to be charmed?
         */
        CharmInfo* InitCharmInfo(Unit* charm);

        /** 
         * Get's the \ref ObjectGuid for a certain totem type that this \ref Unit has spawned
         * @param slot the slot to get the \ref ObjectGuid for
         * @return the \ref ObjectGuid for the given totem slot
         */
        ObjectGuid const& GetTotemGuid(TotemSlot slot) const { return m_TotemSlot[slot]; }
        /** 
         * Gets a certain \ref Totem that this \ref Unit has spawned
         * @param slot the slot to get the \ref Totem for
         * @return The requested totem if there is any spawned, NULL otherwise
         */
        Totem* GetTotem(TotemSlot slot) const;
        /** 
         * @return True if all totems slots are used (spawned), false otherwise
         */
        bool IsAllTotemSlotsUsed() const;

        /** 
         * This is internal code that should only be called from the \ref Totem summon code
         * @param slot 
         * @param totem
         * \internal
         */
        void _AddTotem(TotemSlot slot, Totem* totem);       // only for call from Totem summon code
        /** 
         * This is internal code that should only be called from the \ref Totem class.
         * @param totem
         * \internal
         */
        void _RemoveTotem(Totem* totem);                    // only for call from Totem class

        /** 
         * This will call the given function for all controlled \ref Unit s, for an example of
         * how one such function could look please have a look at \ref CallForAllControlledUnitsExample
         *
         * The functors operator() should have the following signature:
         * \code{.cpp}
         * void operator()(Unit* unit) { ... };
         * \endcode
         * @param func the functor object used to call for each \ref Unit that we will find matching
         * the mask
         * @param controlledMask a mask telling which of the controlled \ref Unit s we want to call
         * the functor for
         */
        template<typename Func>
        void CallForAllControlledUnits(Func const& func, uint32 controlledMask);
        /** 
         * Works pretty much the same way as \ref Unit::CallForAllControlledUnits but instead
         * the functors operator() should have the following signature:
         * \code{.cpp}
         * bool operator()(Unit* unit) { ... };
         * \endcode
         * @param func a functor object used to call for each \ref Unit that we will find matching
         * the mask
         * @param controlledMask a mask telling which of the controlled \ref Unit s we want to call
         * the functor for
         * @return true if the functor returned true for one of the \ref Unit s included by the
         * controlledMask, false if none of them returned true
         * \see Unit::isAttackingPlayer
         */
        template<typename Func>
        bool CheckAllControlledUnits(Func const& func, uint32 controlledMask) const;

        /** 
         * Adds a \ref SpellAuraHolder
         * @param holder the holder to add
         * @return true if the holder was added, false otherwise
         */
        bool AddSpellAuraHolder(SpellAuraHolder* holder);
        /** 
         * Adds a \ref Aura to \ref Unit::m_modAuras
         * @param aura the \ref Aura to add
         */
        void AddAuraToModList(Aura* aura);
        
        
        /** 
         * Removes an \ref Aura and sets the reason for removal inside the \ref Aura.
         * 
         * removing specific aura stack (From old comment)
         * @param aura the \ref Aura to remove
         * @param mode the reason why it is being removed
         * \see Aura::SetRemoveMode
         */
        void RemoveAura(Aura* aura, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        /** 
         * Removes an \ref Aura by spell id and the effect index for that spell to find out
         * which \ref Aura to remove.
         * @param spellId id of the spell which has the sought \ref Aura somewhere
         * @param effindex the effect index for the spell to find the right \ref Aura
         * @param except if != NULL we will not remove this \ref Aura if found
         */
        void RemoveAura(uint32 spellId, SpellEffectIndex effindex, Aura* except = NULL);
        /** 
         * Removes a \ref SpellAuraHolder from this \ref Unit. This will remove all the effects that
         * are currently stored in the \ref SpellAuraHolder.
         * @param holder holder to be removed
         * @param mode reason for removal
         */
        void RemoveSpellAuraHolder(SpellAuraHolder* holder, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        /** 
         * Removes a single \ref Aura from a \ref SpellAuraHolder to cancel out just one effect of a
         * \ref Spell.
         * @param holder the holder to remove the \ref Aura from
         * @param index the effect index to tell which \ref Aura we want to remove
         * @param mode the reason for removing it
         */
        void RemoveSingleAuraFromSpellAuraHolder(SpellAuraHolder* holder, SpellEffectIndex index, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        /** 
         * Does the same thing as \ref Unit::RemoveSingleAuraFromSpellAuraHolder but with spell id
         * instead of a \ref SpellAuraHolder
         * @param id id of the spell to find the \ref Aura in
         * @param index the effect index to tell which \ref Aura we want to remove
         * @param casterGuid guid of the caster to filter it out to just one \ref Aura to remove
         * @param mode reason for removal
         */
        void RemoveSingleAuraFromSpellAuraHolder(uint32 id, SpellEffectIndex index, ObjectGuid casterGuid, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        /** 
         * Removes all \ref Aura s that a certain spell would cause via it's effects (up to 3 of them
         * per \ref Aura).
         * 
         * From old doc: removing specific aura stacka by diff reasons and selections
         * @param spellId id of the spell causing the \ref Aura s you would like to remove
         * @param except a spell that shouldn't be included in the removal
         * @param mode reason for removal
         * \see SpellEntry::Effect
         */
        void RemoveAurasDueToSpell(uint32 spellId, SpellAuraHolder* except = NULL, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        /** 
         * Removes all \ref Aura s that a certain spell cast by a certain \ref Item would cause via
         * it's effects (up to 3 of them per \ref Aura).
         * @param castItem the \ref Item that cast the spell
         * @param spellId id of the spell causing the \ref Aura s you would like to remove
         */
        void RemoveAurasDueToItemSpell(Item* castItem, uint32 spellId);
        /** 
         * Removes all \ref Aura s that a certain spell cast by a certain \ref Player / \ref Unit
         * would cause via it's effects (up to 3 of them per \ref Aura)
         * @param spellId id of the \ref Spell causing the \ref Aura s you would like to remove
         * @param casterGuid \ref ObjectGuid of the caster
         * @param mode reason for removal
         */
        void RemoveAurasByCasterSpell(uint32 spellId, ObjectGuid casterGuid);
        /** 
         * Removes all \ref Aura s caused by a certain spell because it was canceled.
         * @param spellId id of the \ref Spell causing the \ref Aura s you would like to remove
         */
        void RemoveAurasDueToSpellByCancel(uint32 spellId);

        // removing unknown aura stacks by diff reasons and selections
        /**
         * From old doc: removing unknown aura stacks by diff reasons and selections
         * \todo Document and find out what it does
         */
        void RemoveNotOwnTrackedTargetAuras();
        /** 
         * Removes all \ref SpellAuraHolder s that have the given \ref Mechanics mask which is created
         * by doing something like the following if we want a mask for \ref Mechanics::MECHANIC_SAPPED:
         * \code{.cpp}
         * uint32 mask = 1 << (MECHANIC_SAPPED - 1);
         * \endcode
         * @param mechMask a mask of \ref Mechanics, see \ref MECHANIC_NOT_REMOVED_BY_SHAPESHIFT,
         * \ref IMMUNE_TO_ROOT_AND_SNARE_MASK for examples
         * @param exceptSpellId id of a \ref Spell that shouldn't be removed
         * @param non_positive if we should remove non positive \ref Aura s or not, defaults to false
         */
        void RemoveAurasAtMechanicImmunity(uint32 mechMask, uint32 exceptSpellId, bool non_positive = false);
        /** 
         * Removes all \ref Spell s that cause the given \ref AuraType
         * @param auraType the type of auras we would like to remove spells for
         */
        void RemoveSpellsCausingAura(AuraType auraType);
        /** 
         * Same as \ref Unit::RemoveSpellsCausingAura but with an exception
         * for a \ref SpellAuraHolder that shouldn't be removed
         * @param auraType the type of auras we would like to remove spells for
         * @param except this will be excepted from removal
         */
        void RemoveSpellsCausingAura(AuraType auraType, SpellAuraHolder* except);
        /** 
         * Same as \ref Unit::RemoveSpellsCausingAura but for a matching caster aswell.
         * @param auraType the type of auras we would like to remove spells for
         * @param casterGuid remove the aura only if the caster is equal to this guid
         */
        void RemoveSpellsCausingAura(AuraType auraType, ObjectGuid casterGuid);
        /** 
         * Removes all ranks of the given \ref Spell, ie: if the spellid of rank 1 inner fire is
         * given all the ranks of it will be removed.
         * @param spellId id of the spell we want to remove all ranks for
         */
        void RemoveRankAurasDueToSpell(uint32 spellId);
        /** 
         * 
         * @param holder 
         * @return true if we could remove something (and did), false otherwise
         * \todo Document what this does and break into smaller functions!
         */
        bool RemoveNoStackAurasDueToAuraHolder(SpellAuraHolder* holder);
        /** 
         * Removes all \ref Aura s that have the given interrupt flags
         * @param flags see \ref AuraInterruptFlags for possible flags
         */
        void RemoveAurasWithInterruptFlags(uint32 flags);
        /** 
         * Removes all \ref Aura s that have the given attributes
         * @param flags see \ref SpellAttributes for possible values
         */
        void RemoveAurasWithAttribute(uint32 flags);
        /** 
         * Removes all \ref Aura s which can be dispelled by the given \ref DispelType
         * @param type the given type that you want to remove all \ref Aura s for
         * @param casterGuid if this isn't 0 it will be checked that the caster of the \ref Spell is
         * the same as the given guid before removal.
         */
        void RemoveAurasWithDispelType(DispelType type, ObjectGuid casterGuid = ObjectGuid());
        /** 
         * Removes all \ref Aura s.
         * @param mode the reason for removal
         */
        void RemoveAllAuras(AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        /** 
         * Removes all \ref Aura s on this \ref Unit s death. Removes all visible \ref Aura s and
         * disabled the mods for the passive ones (taken from old docs). The reason used is
         * \ref AuraRemoveMode::AURA_REMOVE_BY_DEATH
         * \todo Where does it remove the passive ones?
         */
        void RemoveAllAurasOnDeath();
        /** 
         * used when evading to remove all auras except some special auras. Linked and flying
         * \ref Aura s shouldn't be removed on evade.
         * \todo Are linked and flying auras really not removed on evade?
         */
        void RemoveAllAurasOnEvade();
        
        // removing specific aura FROM stack by diff reasons and selections
        void RemoveAuraHolderFromStack(uint32 spellId, uint32 stackAmount = 1, ObjectGuid casterGuid = ObjectGuid(), AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveAuraHolderDueToSpellByDispel(uint32 spellId, uint32 stackAmount, ObjectGuid casterGuid, Unit* dispeller);

        void DelaySpellAuraHolder(uint32 spellId, int32 delaytime, ObjectGuid casterGuid);

        void SetCreateStat(Stats stat, float val) { m_createStats[stat] = val; }
        void SetCreateHealth(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_HEALTH, val); }
        uint32 GetCreateHealth() const { return GetUInt32Value(UNIT_FIELD_BASE_HEALTH); }
        void SetCreateMana(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_MANA, val); }
        uint32 GetCreateMana() const { return GetUInt32Value(UNIT_FIELD_BASE_MANA); }
        uint32 GetCreatePowers(Powers power) const;
        float GetCreateStat(Stats stat) const { return m_createStats[stat]; }

        void SetCurrentCastedSpell(Spell* pSpell);
        virtual void ProhibitSpellSchool(SpellSchoolMask /*idSchoolMask*/, uint32 /*unTimeMs*/) { }
        void InterruptSpell(CurrentSpellTypes spellType, bool withDelayed = true);
        void FinishSpell(CurrentSpellTypes spellType, bool ok = true);

        // set withDelayed to true to account delayed spells as casted
        // delayed+channeled spells are always accounted as casted
        // we can skip channeled or delayed checks using flags
        bool IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled = false, bool skipAutorepeat = false) const;

        // set withDelayed to true to interrupt delayed spells too
        // delayed+channeled spells are always interrupted
        void InterruptNonMeleeSpells(bool withDelayed, uint32 spellid = 0);

        Spell* GetCurrentSpell(CurrentSpellTypes spellType) const { return m_currentSpells[spellType]; }
        Spell* GetCurrentSpell(uint32 spellType) const { return m_currentSpells[spellType]; }
        Spell* FindCurrentSpellBySpellId(uint32 spell_id) const;

        bool CheckAndIncreaseCastCounter();
        void DecreaseCastCounter() { if (m_castCounter) { --m_castCounter; } }

        ObjectGuid m_ObjectSlotGuid[4];
        uint32 m_detectInvisibilityMask;
        uint32 m_invisibilityMask;

        ShapeshiftForm GetShapeshiftForm() const { return ShapeshiftForm(GetByteValue(UNIT_FIELD_BYTES_1, 2)); }
        void  SetShapeshiftForm(ShapeshiftForm form) { SetByteValue(UNIT_FIELD_BYTES_1, 2, form); }

        bool IsInFeralForm() const
        {
            ShapeshiftForm form = GetShapeshiftForm();
            return form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR;
        }

        bool IsInDisallowedMountForm() const
        {
            ShapeshiftForm form = GetShapeshiftForm();
            return form != FORM_NONE && form != FORM_BATTLESTANCE && form != FORM_BERSERKERSTANCE && form != FORM_DEFENSIVESTANCE &&
                   form != FORM_SHADOW && form != FORM_STEALTH; 
        }

        float m_modMeleeHitChance;
        float m_modRangedHitChance;
        float m_modSpellHitChance;
        int32 m_baseSpellCritChance;

        float m_threatModifier[MAX_SPELL_SCHOOL];
        float m_modAttackSpeedPct[3];

        // Event handler
        EventProcessor m_Events;

        // stat system
        bool HandleStatModifier(UnitMods unitMod, UnitModifierType modifierType, float amount, bool apply);
        void SetModifierValue(UnitMods unitMod, UnitModifierType modifierType, float value) { m_auraModifiersGroup[unitMod][modifierType] = value; }
        float GetModifierValue(UnitMods unitMod, UnitModifierType modifierType) const;
        float GetTotalStatValue(Stats stat) const;
        float GetTotalAuraModValue(UnitMods unitMod) const;
        SpellSchools GetSpellSchoolByAuraGroup(UnitMods unitMod) const;
        Stats GetStatByAuraGroup(UnitMods unitMod) const;
        Powers GetPowerTypeByAuraGroup(UnitMods unitMod) const;
        bool CanModifyStats() const { return m_canModifyStats; }
        void SetCanModifyStats(bool modifyStats) { m_canModifyStats = modifyStats; }
        virtual bool UpdateStats(Stats stat) = 0;
        virtual bool UpdateAllStats() = 0;
        virtual void UpdateResistances(uint32 school) = 0;
        virtual void UpdateArmor() = 0;
        virtual void UpdateMaxHealth() = 0;
        virtual void UpdateMaxPower(Powers power) = 0;
        virtual void UpdateAttackPowerAndDamage(bool ranged = false) = 0;
        virtual void UpdateDamagePhysical(WeaponAttackType attType) = 0;
        float GetTotalAttackPowerValue(WeaponAttackType attType) const;
        float GetWeaponDamageRange(WeaponAttackType attType , WeaponDamageRange type) const;
        void SetBaseWeaponDamage(WeaponAttackType attType , WeaponDamageRange damageRange, float value) { m_weaponDamage[attType][damageRange] = value; }

        // Visibility system
        UnitVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(UnitVisibility x);
        void UpdateVisibilityAndView() override;            // overwrite WorldObject::UpdateVisibilityAndView()

        // common function for visibility checks for player/creatures with detection code
        bool IsVisibleForOrDetect(Unit const* u, WorldObject const* viewPoint, bool detect, bool inVisibleList = false, bool is3dDistance = true) const;
        bool CanDetectInvisibilityOf(Unit const* u) const;

        // virtual functions for all world objects types
        bool IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;
        // function for low level grid visibility checks in player/creature cases
        virtual bool IsVisibleInGridForPlayer(Player* pl) const = 0;
        bool IsInvisibleForAlive() const;

        TrackedAuraTargetMap&       GetTrackedAuraTargets(TrackedAuraType type)       { return m_trackedAuraTargets[type]; }
        TrackedAuraTargetMap const& GetTrackedAuraTargets(TrackedAuraType type) const { return m_trackedAuraTargets[type]; }
        SpellImmuneList m_spellImmune[MAX_SPELL_IMMUNITY];

        // Threat related methods
        bool CanHaveThreatList(bool ignoreAliveState = false) const;
        void AddThreat(Unit* pVictim, float threat = 0.0f, bool crit = false, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NONE, SpellEntry const* threatSpell = NULL);
        float ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL);
        void DeleteThreatList();
        bool IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea);
        bool SelectHostileTarget();
        void TauntApply(Unit* pVictim);
        void TauntFadeOut(Unit* taunter);
        void FixateTarget(Unit* pVictim);
        ObjectGuid GetFixateTargetGuid() const { return m_fixateTargetGuid; }
        ThreatManager& GetThreatManager() { return m_ThreatManager; }
        ThreatManager const& GetThreatManager() const { return m_ThreatManager; }
        void AddHatedBy(HostileReference* pHostileReference) { m_HostileRefManager.insertFirst(pHostileReference); };
        void RemoveHatedBy(HostileReference* /*pHostileReference*/) { /* nothing to do yet */ }
        HostileRefManager& GetHostileRefManager() { return m_HostileRefManager; }

        Aura* GetAura(uint32 spellId, SpellEffectIndex effindex);
        Aura* GetAura(AuraType type, SpellFamily family, uint64 familyFlag, ObjectGuid casterGuid = ObjectGuid());
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid) const;
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid, ObjectGuid casterGUID) const;

        SpellAuraHolderMap&       GetSpellAuraHolderMap()       { return m_spellAuraHolders; }
        SpellAuraHolderMap const& GetSpellAuraHolderMap() const { return m_spellAuraHolders; }
        /**
         * Get's a list of all the \ref Aura s of the given \ref AuraType that are currently
         * affecting this \ref Unit.
         * @param type the aura type we want to find
         * @return A list of the auras currently applied to the \ref Unit with the given \ref AuraType
         * \see Unit::m_modAuras
         */
        AuraList const& GetAurasByType(AuraType type) const { return m_modAuras[type]; }
        void ApplyAuraProcTriggerDamage(Aura* aura, bool apply);

        int32 GetTotalAuraModifier(AuraType auratype) const;
        float GetTotalAuraMultiplier(AuraType auratype) const;
        int32 GetMaxPositiveAuraModifier(AuraType auratype) const;
        int32 GetMaxNegativeAuraModifier(AuraType auratype) const;

        int32 GetTotalAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        float GetTotalAuraMultiplierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxPositiveAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxNegativeAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;

        int32 GetTotalAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        float GetTotalAuraMultiplierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxPositiveAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxNegativeAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;

        Aura* GetDummyAura(uint32 spell_id) const;

        uint32 m_AuraFlags;

        uint32 GetDisplayId() const { return GetUInt32Value(UNIT_FIELD_DISPLAYID); }
        void SetDisplayId(uint32 modelId);
        uint32 GetNativeDisplayId() const { return GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID); }
        void SetNativeDisplayId(uint32 modelId) { SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, modelId); }
        void SetTransform(uint32 spellid) { m_transform = spellid;}
        uint32 GetTransform() const { return m_transform;}

        // at any changes to scale and/or displayId
        void UpdateModelData();
        float GetObjectScaleMod() const;

        DynamicObject* GetDynObject(uint32 spellId, SpellEffectIndex effIndex);
        DynamicObject* GetDynObject(uint32 spellId);
        void AddDynObject(DynamicObject* dynObj);
        void RemoveDynObject(uint32 spellid);
        void RemoveDynObjectWithGUID(ObjectGuid guid) { m_dynObjGUIDs.remove(guid); }
        void RemoveAllDynObjects();

        GameObject* GetGameObject(uint32 spellId) const;
        void AddGameObject(GameObject* gameObj);
        void AddWildGameObject(GameObject* gameObj);
        void RemoveGameObject(GameObject* gameObj, bool del);
        void RemoveGameObject(uint32 spellid, bool del);
        void RemoveAllGameObjects();

        uint32 CalculateDamage(WeaponAttackType attType, bool normalized);
        float GetAPMultiplier(WeaponAttackType attType, bool normalized);
        void ModifyAuraState(AuraState flag, bool apply);
        bool HasAuraState(AuraState flag) const { return HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1)); }
        void UnsummonAllTotems();
        Unit* SelectMagnetTarget(Unit* victim, Spell* spell = NULL, SpellEffectIndex eff = EFFECT_INDEX_0);

        int32 SpellBonusWithCoeffs(Unit* pCaster, SpellEntry const* spellProto, int32 total, int32 benefit, int32 ap_benefit, DamageEffectType damagetype, bool donePart);
        int32 SpellBaseDamageBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseDamageBonusTaken(SpellSchoolMask schoolMask);
        uint32 SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        uint32 SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        int32 SpellBaseHealingBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask);
        uint32 SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
        uint32 SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
        uint32 MeleeDamageBonusDone(Unit* pVictim, uint32 damage, WeaponAttackType attType, SpellEntry const* spellProto = NULL, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);
        uint32 MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto = NULL, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);

        bool   IsSpellBlocked(Unit* pCaster, SpellEntry const* spellProto, WeaponAttackType attackType = BASE_ATTACK);
        bool   IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto, SpellSchoolMask schoolMask, WeaponAttackType attackType = BASE_ATTACK);
        uint32 SpellCriticalDamageBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim);
        uint32 SpellCriticalHealingBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim);

        bool IsTriggeredAtSpellProcEvent(Unit* pVictim, SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, bool isVictim, SpellProcEventEntry const*& spellProcEvent);
        // Aura proc handlers
        SpellAuraProcResult HandleDummyAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleHasteAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleProcTriggerSpellAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleProcTriggerDamageAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleOverrideClassScriptAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModCastingSpeedNotStackAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleReflectSpellsSchoolAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModPowerCostSchoolAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleMechanicImmuneResistanceAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleModResistanceAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleRemoveByDamageChanceProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleInvisibilityAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown);
        SpellAuraProcResult HandleNULLProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* /*triggeredByAura*/, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
        {
            // no proc handler for this aura type
            return SPELL_AURA_PROC_OK;
        }
        SpellAuraProcResult HandleCantTrigger(Unit* /*pVictim*/, uint32 /*damage*/, Aura* /*triggeredByAura*/, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
        {
            // this aura type can't proc
            return SPELL_AURA_PROC_CANT_TRIGGER;
        }

        void SetLastManaUse()
        {
            m_lastManaUseTimer = 5000;
        }
        bool IsUnderLastManaUseEffect() const { return m_lastManaUseTimer; }

        void SetContestedPvP(Player* attackedPlayer = NULL);

        void ApplySpellImmune(uint32 spellId, uint32 op, uint32 type, bool apply);
        void ApplySpellDispelImmunity(const SpellEntry* spellProto, DispelType type, bool apply);
        virtual bool IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf);
        bool IsImmunedToDamage(SpellSchoolMask meleeSchoolMask);
        virtual bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const;

        uint32 CalcArmorReducedDamage(Unit* pVictim, const uint32 damage);
        void CalculateDamageAbsorbAndResist(Unit* pCaster, SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32* absorb, uint32* resist, bool canReflect = false);
        void CalculateAbsorbResistBlock(Unit* pCaster, SpellNonMeleeDamage* damageInfo, SpellEntry const* spellProto, WeaponAttackType attType = BASE_ATTACK);

        virtual void UpdateSpeed(UnitMoveType mtype, bool forced, float ratio = 1.0f);
        float GetSpeed(UnitMoveType mtype) const;
        float GetSpeedRate(UnitMoveType mtype) const { return m_speed_rate[mtype]; }
        void SetSpeedRate(UnitMoveType mtype, float rate, bool forced = false);

        bool IsHover() const { return HasAuraType(SPELL_AURA_HOVER); }

        void _RemoveAllAuraMods();
        void _ApplyAllAuraMods();

        int32 CalculateSpellDamage(Unit const* target, SpellEntry const* spellProto, SpellEffectIndex effect_index, int32 const* basePoints = NULL);

        float CalculateLevelPenalty(SpellEntry const* spellProto) const;

        void AddFollower(FollowerReference* pRef) { m_FollowingRefManager.insertFirst(pRef); }
        void RemoveFollower(FollowerReference* /*pRef*/) { /* nothing to do yet */ }

        MotionMaster* GetMotionMaster() { return &i_motionMaster; }

        bool IsStopped() const { return !(hasUnitState(UNIT_STAT_MOVING)); }
        void StopMoving(bool forceSendStop = false);
        void InterruptMoving(bool forceSendStop = false);

        void SetFeared(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0, uint32 time = 0);
        void SetConfused(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0);
        void SetFeignDeath(bool apply, ObjectGuid casterGuid = ObjectGuid());

        void AddComboPointHolder(uint32 lowguid) { m_ComboPointHolders.insert(lowguid); }
        void RemoveComboPointHolder(uint32 lowguid) { m_ComboPointHolders.erase(lowguid); }
        void ClearComboPointHolders();

        ///----------Pet responses methods-----------------
        void SendPetCastFail(uint32 spellid, SpellCastResult msg);
        void SendPetActionFeedback(uint8 msg);
        void SendPetTalk(uint32 pettalk);
        void SendPetAIReaction();
        ///----------End of Pet responses methods----------

        void PropagateSpeedChange() { GetMotionMaster()->PropagateSpeedChange(); }

        // reactive attacks
        void ClearAllReactives();
        void StartReactiveTimer(ReactiveType reactive) { m_reactiveTimer[reactive] = REACTIVE_TIMER_START;}
        void UpdateReactives(uint32 p_time);

        // group updates
        void UpdateAuraForGroup(uint8 slot);

        // pet auras
        typedef std::set<PetAura const*> PetAuraSet;
        PetAuraSet m_petAuras;
        void AddPetAura(PetAura const* petSpell);
        void RemovePetAura(PetAura const* petSpell);

        // Movement info
        MovementInfo m_movementInfo;
        Movement::MoveSpline* movespline;

        void ScheduleAINotify(uint32 delay);
        bool IsAINotifyScheduled() const { return m_AINotifyScheduled;}
        void _SetAINotifyScheduled(bool on) { m_AINotifyScheduled = on;}       // only for call from RelocationNotifyEvent code
        void OnRelocated();

        bool IsLinkingEventTrigger() { return m_isCreatureLinkingTrigger; }

        virtual bool CanSwim() const = 0;
        virtual bool CanFly() const = 0;

    protected:
        explicit Unit();

        void _UpdateSpells(uint32 time);
        void _UpdateAutoRepeatSpell();
        bool m_AutoRepeatFirstCast;

        uint32 m_attackTimer[MAX_ATTACK];

        float m_createStats[MAX_STATS];

        AttackerSet m_attackers;
        Unit* m_attacking;

        DeathState m_deathState; ///< The current state of life/death for this \ref Unit

        SpellAuraHolderMap m_spellAuraHolders;
        SpellAuraHolderMap::iterator m_spellAuraHoldersUpdateIterator; // != end() in Unit::m_spellAuraHolders update and point to next element
        AuraList m_deletedAuras;                            // auras removed while in ApplyModifier and waiting deleted
        SpellAuraHolderList m_deletedHolders;

        // Store Auras for which the target must be tracked
        TrackedAuraTargetMap m_trackedAuraTargets[MAX_TRACKED_AURA_TYPES];

        GuidList m_dynObjGUIDs;

        typedef std::list<GameObject*> GameObjectList;
        GameObjectList m_gameObj;
        typedef std::map<uint32, ObjectGuid> WildGameObjectMap;
        WildGameObjectMap m_wildGameObjs;
        bool m_isSorted;
        uint32 m_transform;

        AuraList m_modAuras[TOTAL_AURAS];
        float m_auraModifiersGroup[UNIT_MOD_END][MODIFIER_TYPE_END];
        float m_weaponDamage[MAX_ATTACK][2];
        bool m_canModifyStats;
        // std::list< spellEffectPair > AuraSpells[TOTAL_AURAS];  // TODO: use this if ok for mem

        float m_speed_rate[MAX_MOVE_TYPE];

        CharmInfo* m_charmInfo;

        virtual SpellSchoolMask GetMeleeDamageSchoolMask() const;

        MotionMaster i_motionMaster;

        uint32 m_reactiveTimer[MAX_REACTIVE];
        uint32 m_regenTimer;
        uint32 m_lastManaUseTimer;

        void DisableSpline();
        bool m_isCreatureLinkingTrigger;
        bool m_isSpawningLinked;

    private:
        void CleanupDeletedAuras();
        void UpdateSplineMovement(uint32 t_diff);

        Unit* _GetTotem(TotemSlot slot) const;              // for templated function without include need
        Pet* _GetPet(ObjectGuid guid) const;                // for templated function without include need

        // Wrapper called by DealDamage when a creature is killed
        void JustKilledCreature(Creature* victim, Player* responsiblePlayer);

        uint32 m_state;                                     // Even derived shouldn't modify
        uint32 m_CombatTimer;
        bool   m_dummyCombatState;                          // Used to keep combat state during some aura

        Spell* m_currentSpells[CURRENT_MAX_SPELL];
        uint32 m_castCounter;                               // count casts chain of triggered spells for prevent infinity cast crashes

        UnitVisibility m_Visibility;
        Position m_last_notified_position;
        bool m_AINotifyScheduled;
        ShortTimeTracker m_movesplineTimer;

        Diminishing m_Diminishing;
        // Manage all Units threatening us
        ThreatManager m_ThreatManager;
        // Manage all Units that are threatened by us
        HostileRefManager m_HostileRefManager;

        FollowerRefManager m_FollowingRefManager;

        ComboPointHolderSet m_ComboPointHolders;

        GuidSet m_guardianPets;

        ObjectGuid m_TotemSlot[MAX_TOTEM_SLOT];

        ObjectGuid m_fixateTargetGuid;                      //< Stores the Guid of a fixated target

    private:                                                // Error traps for some wrong args using
        // this will catch and prevent build for any cases when all optional args skipped and instead triggered used non boolean type
        // no bodies expected for this declarations
        template <typename TR>
        void CastSpell(Unit* Victim, uint32 spell, TR triggered);
        template <typename TR>
        void CastSpell(Unit* Victim, SpellEntry const* spell, TR triggered);
        template <typename TR>
        void CastCustomSpell(Unit* Victim, uint32 spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggered);
        template <typename SP, typename TR>
        void CastCustomSpell(Unit* Victim, SpellEntry const* spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggered);
        template <typename TR>
        void CastSpell(float x, float y, float z, uint32 spell, TR triggered);
        template <typename TR>
        void CastSpell(float x, float y, float z, SpellEntry const* spell, TR triggered);
};

template<typename Func>
void Unit::CallForAllControlledUnits(Func const& func, uint32 controlledMask)
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet* pet = GetPet())
            { func(pet); }

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet* mini = GetMiniPet())
            { func(mini); }

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (GuidSet::const_iterator itr = m_guardianPets.begin(); itr != m_guardianPets.end();)
            if (Pet* guardian = _GetPet(*(itr++)))
                { func(guardian); }
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit* totem = _GetTotem(TotemSlot(i)))
                { func(totem); }
    }

    if (controlledMask & CONTROLLED_CHARM)
        if (Unit* charm = GetCharm())
            { func(charm); }
}


template<typename Func>
bool Unit::CheckAllControlledUnits(Func const& func, uint32 controlledMask) const
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet const* pet = GetPet())
            if (func(pet))
                { return true; }

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet* mini = GetMiniPet())
            if (func(mini))
                { return true; }

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (GuidSet::const_iterator itr = m_guardianPets.begin(); itr != m_guardianPets.end();)
            if (Pet const* guardian = _GetPet(*(itr++)))
                if (func(guardian))
                    { return true; }
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit const* totem = _GetTotem(TotemSlot(i)))
                if (func(totem))
                    { return true; }
    }

    if (controlledMask & CONTROLLED_CHARM)
        if (Unit const* charm = GetCharm())
            if (func(charm))
                { return true; }

    return false;
}

/** @} */

#endif
