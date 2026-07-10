#pragma once

#include "../Action.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class MovementAction : public Action {
        public:
            MovementAction(PlayerbotAI* ai, string name) : Action(ai, name)
            {
                bot = ai->GetBot();
                transportBoardingDelayTime = 0;
            }

        protected:
            bool MoveNear(uint32 mapId, float x, float y, float z, float distance = sPlayerbotAIConfig.followDistance);
            bool MoveTo(uint32 mapId, float x, float y, float z, bool unsafe = false);
            bool MoveTo(Unit* target, float distance = 0.0f);
            bool MoveNear(WorldObject* target, float distance = sPlayerbotAIConfig.followDistance);
            float GetFollowAngle();
            bool Follow(Unit* target, float distance = sPlayerbotAIConfig.followDistance);
            bool Follow(Unit* target, float distance, float angle);
            bool FollowOnTransport(Unit* target, Player* master);
            bool FollowOffTransport(Unit* target, Player* master);
            void WaitForReach(float distance);
            bool IsMovingAllowed(Unit* target);
            bool IsMovingAllowed(uint32 mapId, float x, float y, float z);
            bool IsMovingAllowed();
            bool Flee(Unit *target);
            float CalculateAggroFreeDistance(float bx, float by, float angle, float maxDist);
            bool IsAggroPosition(float x, float y);
            bool FindNearbyLosPoint(Unit* target, float& nx, float& ny,
                                    float& nz, float maxRadius = 20.0f);

        protected:
            Player* bot;
            uint32 transportBoardingDelayTime;
    };

    class FleeAction : public MovementAction
    {
        public:
            FleeAction(PlayerbotAI* ai, float distance = sPlayerbotAIConfig.spellDistance) : MovementAction(ai, "flee")
            {
                this->distance = distance;
            }

            virtual bool Execute(Event event);
            virtual bool isUseful();

        private:
            float distance;
    };

    class RunAwayAction : public MovementAction
    {
        public:
            RunAwayAction(PlayerbotAI* ai) : MovementAction(ai, "runaway") {}
            virtual bool Execute(Event event);
    };

    class MoveRandomAction : public MovementAction
    {
        public:
            MoveRandomAction(PlayerbotAI* ai) : MovementAction(ai, "move random"), m_hasFaceTarget(false), m_faceX(0.0f), m_faceY(0.0f) {}
            virtual bool Execute(Event event);
            virtual bool isPossible()
            {
                return !bot->GetGroup() &&
                    MovementAction::isPossible() &&
                    AI_VALUE2(uint8, "health", "self target") > sPlayerbotAIConfig.mediumHealth &&
                    (!AI_VALUE2(uint8, "mana", "self target") || AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.mediumMana);
            }
        private:
            bool m_hasFaceTarget;
            float m_faceX, m_faceY;
    };

    class MoveToLootAction : public MovementAction
    {
        public:
            MoveToLootAction(PlayerbotAI* ai) : MovementAction(ai, "move to loot") {}
            virtual bool Execute(Event event);
    };

    class MoveOutOfEnemyContactAction : public MovementAction
    {
        public:
            MoveOutOfEnemyContactAction(PlayerbotAI* ai) : MovementAction(ai, "move out of enemy contact") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class SetFacingTargetAction : public MovementAction
    {
        public:
            SetFacingTargetAction(PlayerbotAI* ai) : MovementAction(ai, "set facing") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class JumpAction : public MovementAction
    {
        public:
            JumpAction(PlayerbotAI* ai) : MovementAction(ai, "jump") {}
            virtual bool Execute(Event event);
    };

    class SwimToSurfaceAction : public MovementAction
    {
        public:
            SwimToSurfaceAction(PlayerbotAI* ai) : MovementAction(ai, "swim to surface") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };
}
