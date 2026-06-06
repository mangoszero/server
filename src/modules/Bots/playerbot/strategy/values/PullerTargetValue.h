#pragma once
#include "../Value.h"

namespace ai
{
    class PullerTargetValue : public UnitCalculatedValue
    {
        public:
            PullerTargetValue(PlayerbotAI* ai) : UnitCalculatedValue(ai) {}

            virtual Unit* Calculate()
            {
                Player *bot = ai->GetBot();
                if (!bot)
                {
                    return NULL;
                }
                if (ai->HasStrategy("pull", BOT_STATE_COMBAT))
                {
                    return bot;
                }
                Group *group = bot->GetGroup();
                if (group)
                {
                    for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
                    {
                        Player* member = gref->getSource();
                        if (member &&
                            member->GetPlayerbotAI() &&
                            member->GetPlayerbotAI()->HasStrategy("pull", BOT_STATE_COMBAT))
                        {
                            return member;
                        }
                    }
                }
                return NULL;
            }
    };
}
