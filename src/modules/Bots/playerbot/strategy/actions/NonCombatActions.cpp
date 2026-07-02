#include "botpch.h"
#include "../../playerbot.h"
#include "NonCombatActions.h"

using namespace ai;

bool DrinkAction::Execute(Event event)
{
    if (bot->IsInCombat())
    {
        return false;
    }

    if (ai->IsDrinking())
    {
        return true;
    }

    bool result;
    list<Item*> conjuredDrinks = AI_VALUE2(list<Item*>, "inventory items", "conjured drink");
    if (!conjuredDrinks.empty())
    {
        result = UseItemAuto(*conjuredDrinks.begin());
    }
    else
    {
        result = UseItemAction::Execute(event);
    }
    if (result)
    {
        ai->SetDrinking();
    }
    return result;
}

bool DrinkAction::isPossible()
{
    return ai->IsDrinking() || UseItemAction::isPossible();
}

bool DrinkAction::isUseful()
{
    if (ai->IsDrinking())
    {
        return true;
    }
    return UseItemAction::isUseful() && AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.thirstyMana;
}

bool EatAction::Execute(Event event)
{
    if (bot->IsInCombat())
    {
        return false;
    }

    if (ai->IsEating())
    {
        return true;
    }

    bool result = false;
    list<Item*> buffFoods = AI_VALUE2(list<Item*>, "inventory items", "buff food");
    if (!buffFoods.empty() && !HasFoodBuff(bot, buffFoods))
    {
        result = UseItemAuto(*buffFoods.begin());
    }
    else
    {
        list<Item*> allFoods = AI_VALUE2(list<Item*>, "inventory items", "food");
        if (!allFoods.empty())
        {
            auto it = std::find_if(allFoods.begin(), allFoods.end(),
                [](Item* item)
                {
                    return item && item->GetProto()->IsConjuredConsumable();
                });
            if (it != allFoods.end())
            {
                result = UseItemAuto(*it);
            }
            else
            {
                result = UseItemAuto(*allFoods.begin());
            }
        }
    }

    if (result)
    {
        ai->SetEating();
    }
    return result;
}

bool EatAction::isPossible()
{
    if (ai->IsEating())
    {
        return true;
    }

    if (AI_VALUE2(list<Item*>, "inventory items", "food").empty())
    {
        return false;
    }
    return UseItemAction::isPossible();
}

bool EatAction::isUseful()
{
    if (ai->IsEating())
    {
        return true;
    }
    return UseItemAction::isUseful() && AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.hungryHealth;
}

