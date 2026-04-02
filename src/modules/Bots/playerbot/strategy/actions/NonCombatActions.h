#pragma once

#include "../Action.h"
#include "UseItemAction.h"
#include "../../PlayerbotAIConfig.h"
#include "../ItemVisitors.h"

namespace ai
{
    class DrinkAction : public UseItemAction
    {
    public:
        DrinkAction(PlayerbotAI* ai) : UseItemAction(ai, "drink") {}

        virtual bool Execute(Event event)
        {
            if (bot->IsInCombat())
            {
                return false;
            }

            if (ai->IsDrinking())
                return true;

            bool result = UseItemAction::Execute(event);
            if (result)
                ai->SetDrinking();
            return result;
        }

        virtual bool isPossible()
        {
            return ai->IsDrinking() || UseItemAction::isPossible();
        }

        virtual bool isUseful()
        {
            if (ai->IsDrinking())
                return true;
            return UseItemAction::isUseful() && AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.thirstyMana;
        }
    };

    class EatAction : public UseItemAction
    {
    public:
        EatAction(PlayerbotAI* ai) : UseItemAction(ai, "food") {}

        virtual bool Execute(Event event)
        {
            if (bot->IsInCombat())
            {
                return false;
            }

            if (ai->IsEating())
                return true;

            bool result = false;
            list<Item*> buffFoods = AI_VALUE2(list<Item*>, "inventory items", "buff food");
            if (!buffFoods.empty() && !HasFoodBuff(bot, buffFoods))
                result = UseItemAuto(*buffFoods.begin());
            if (!result)
            {
                list<Item*> foods = AI_VALUE2(list<Item*>, "inventory items", "food");
                if (!foods.empty())
                    result = UseItemAuto(*foods.begin());
            }

            if (result)
                ai->SetEating();
            return result;
        }

        virtual bool isPossible()
        {
            if (ai->IsEating())
                return true;
            if(AI_VALUE2(list<Item*>, "inventory items", "food").empty())
                return false;
            return UseItemAction::isPossible();
        }

        virtual bool isUseful()
        {
            if (ai->IsEating())
                return true;
            return UseItemAction::isUseful() && AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.hungryHealth;
        }
    };

}
