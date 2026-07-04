#pragma once

#include "../Action.h"
#include "MovementActions.h"

namespace ai
{
    enum FishState
    {
        FISH_STATE_IDLE,
        FISH_STATE_EQUIPPING,
        FISH_STATE_MOVING,
        FISH_STATE_CASTING,
        FISH_STATE_WAITING,
        FISH_STATE_LOOTING
    };

    class FishWithMasterAction : public Action
    {
        public:
            FishWithMasterAction(PlayerbotAI* ai);
            virtual bool Execute(Event event);
            virtual bool isUseful();
            virtual bool isPossible();

        private:
            Item* FindFishingPole();
            void SaveWeapons();
            bool AutoEquipFromBag(Item* pItem);
            void EquipFishingPole();
            bool MoveToFishingSpot();
            bool CastFishing();
            bool CheckBobberAndLoot();
            GameObject* GetOwnBobber();
            void CleanupFishing();

            FishState m_state;
            time_t m_stateStart;
    };
}
