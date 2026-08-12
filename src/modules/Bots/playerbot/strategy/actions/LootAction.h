#pragma once

#include "../Action.h"
#include "../../LootObjectStack.h"
#include "MovementActions.h"

namespace ai
{
    class LootAction : public MovementAction
    {
        public:
            LootAction(PlayerbotAI* ai) : MovementAction(ai, "loot") {}
            virtual bool Execute(Event event);
    };

    class OpenLootAction : public MovementAction
    {
        public:
            OpenLootAction(PlayerbotAI* ai) : MovementAction(ai, "open loot"), m_retryStarted(0) {}
            virtual bool Execute(Event event);

        private:
            /**
             * @brief Why an attempt to open a loot object ended.
             *
             * A plain bool could not distinguish "not yet" from "not ever", and that is the
             * whole of the wedge: the caller cleared the loot target only on success, so a
             * target the bot could NEVER open stayed selected forever. "can loot" then stayed
             * true, FollowMasterAction refused to follow while it was, and a grouped bot
             * parked next to a corpse it had no skill for and stopped following its master.
             */
            enum LootResult
            {
                LOOT_OK,        ///< Opened, or the open packet was queued.
                LOOT_RETRY,     ///< Cannot right now -- too far, mid-cast, already looting.
                LOOT_IMPOSSIBLE ///< Cannot ever, as things stand: no skill, no key, nothing there.
            };

            LootResult DoLoot(LootObject& lootObject);

            /**
             * @brief How long one loot target may keep answering "not yet".
             *
             * Retryable is not the same as retryable forever. A cast can fail for a reason
             * that will never clear on its own -- a shapeshifted druid attempting Mining,
             * whose spell carries SPELL_ATTR_NOT_SHAPESHIFT, fails synchronously on every
             * attempt and nothing in the loot path ever unshifts it. Left unbounded that is
             * the original wedge wearing a different hat: the target stays valid, "can loot"
             * stays true, and the bot never follows its master again.
             *
             * Reclassifying every failed cast as impossible would be wrong in the other
             * direction, because failures like "not facing the target" genuinely do clear on
             * the next tick. So retries are allowed, and then they run out.
             */
            static const uint32 LOOT_RETRY_SECONDS = 30;

            ObjectGuid m_retryGuid;    ///< Target currently being retried.
            time_t m_retryStarted;     ///< When retrying of m_retryGuid began.
            uint32 GetOpeningSpell(LootObject& lootObject);
            uint32 GetOpeningSpell(LootObject& lootObject, GameObject* go);
            bool CanOpenLock(LootObject& lootObject, const SpellEntry* pSpellInfo, GameObject* go);
            bool CanOpenLock(uint32 skillId, uint32 reqSkillValue);
    };

    class StoreLootAction : public MovementAction
    {
        public:
            StoreLootAction(PlayerbotAI* ai) : MovementAction(ai, "store loot") {}
            virtual bool Execute(Event event);

        protected:
            bool IsLootAllowed(uint32 itemid);
    };
}
