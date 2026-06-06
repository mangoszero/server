
#pragma once

#include "../Action.h"
#include "GenericSpellActions.h"

namespace ai
{
    /**
     * @brief Causes bot to wait for a target to reach melee, with timeout.
     */
    class WatchTargetApproachAction : public Action
    {
        public:
            WatchTargetApproachAction(PlayerbotAI* ai) : Action(ai, "watch target approach") {}
            virtual bool Execute(Event event);
            virtual void Reset();
            virtual bool isUseful();
            virtual string GetTargetName()
            {
                return "current target";
            }
    };

    /**
     * @brief Causes bot's group to wait for pulling member to complete pull.
     *
     * Includes a 10 second timeout.
     */
    class WatchGroupPullAction : public Action
    {
        public:
            WatchGroupPullAction(PlayerbotAI* ai) : Action(ai, "watch group pull") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
            virtual string GetTargetName()
            {
                return "puller target";
            }
    };

    /**
     * @brief Causes a bot to begin the pull process.
     *
     * This action will add the pull strategy to the bot
     * performing this action, and if they are in a group, it
     * will add the wait for pull strategy to the group members.
     */
    class StartPullAction : public Action
    {
        public:
            StartPullAction(PlayerbotAI* ai) : Action(ai, "start pull") {}
            virtual bool Execute(Event event);
            virtual string GetTargetName()
            {
                return "current target";
            }
    };

    /**
     * @brief Causes a bot to persistently attempt to pull
     *
     * This action will use the correct pull spell to attempt
     * to aggro the target, and will remain passive or retry
     * so long as the target is not aggroed yet.
     */
    class RangedPullAction : public CastSpellAction
    {
        public:
            RangedPullAction(PlayerbotAI* ai);
            virtual bool isUseful();
            virtual bool isPossible() { return true; }
            virtual NextAction** getAlternatives()
            {
                return NULL;
            }
            virtual NextAction** getPrerequisites()
            {
                return NULL;
            }
            virtual bool Execute(Event event);
            virtual string GetTargetName()
            {
                return "current target";
            }
    };
}

