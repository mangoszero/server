/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013  ScriptDev2 <http://www.scriptdev2.com/>
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

#ifndef SC_FOLLOWERAI_H
#define SC_FOLLOWERAI_H

enum FollowState
{
    STATE_FOLLOW_NONE       = 0x000,
    STATE_FOLLOW_INPROGRESS = 0x001,                        // must always have this state for any follow
    STATE_FOLLOW_RETURNING  = 0x002,                        // when returning to combat start after being in combat
    STATE_FOLLOW_PAUSED     = 0x004,                        // disables following
    STATE_FOLLOW_COMPLETE   = 0x008,                        // follow is completed and may end
    STATE_FOLLOW_PREEVENT   = 0x010,                        // not implemented (allow pre event to run, before follow is initiated)
    STATE_FOLLOW_POSTEVENT  = 0x020                         // can be set at complete and allow post event to run
};

class FollowerAI : public ScriptedAI
{
    public:
        explicit FollowerAI(Creature* pCreature);
        ~FollowerAI() {}

        // virtual void WaypointReached(uint32 uiPointId) = 0;

        void MovementInform(uint32 uiMotionType, uint32 uiPointId) override;

        void AttackStart(Unit*) override;

        void MoveInLineOfSight(Unit*) override;

        void EnterEvadeMode() override;

        void JustDied(Unit*) override;

        void JustRespawned() override;

        void UpdateAI(const uint32) override;               // the "internal" update, calls UpdateFollowerAI()
        virtual void UpdateFollowerAI(const uint32);        // used when it's needed to add code in update (abilities, scripted events, etc)

        void StartFollow(Player* pPlayer, uint32 uiFactionForFollower = 0, const Quest* pQuest = NULL);

        void SetFollowPaused(bool bPaused);                 // if special event require follow mode to hold/resume during the follow
        void SetFollowComplete(bool bWithEndEvent = false);

        bool HasFollowState(uint32 uiFollowState) { return (m_uiFollowState & uiFollowState); }

    protected:
        Player* GetLeaderForFollower();

    private:
        void AddFollowState(uint32 uiFollowState) { m_uiFollowState |= uiFollowState; }
        void RemoveFollowState(uint32 uiFollowState) { m_uiFollowState &= ~uiFollowState; }

        bool AssistPlayerInCombat(Unit* pWho);

        ObjectGuid m_leaderGuid;
        uint32 m_uiUpdateFollowTimer;
        uint32 m_uiFollowState;

        const Quest* m_pQuestForFollow;                     // normally we have a quest
};

#endif
