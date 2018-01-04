#include "../../botpch.h"
#include "../playerbot.h"
#include "Action.h"
#include "Queue.h"

using namespace ai;


void Queue::Push(ActionBasket *action)
{
    if (action)
    {
        for (std::list<ActionBasket*>::iterator iter = actions.begin(); iter != actions.end(); iter++)
        {
            ActionBasket* basket = *iter;
            if (action->getAction()->getName() == basket->getAction()->getName())
            {
                if (basket->getRelevance() < action->getRelevance())
                    basket->setRelevance(action->getRelevance());
                delete action;
                return;
            }
        }
        actions.push_back(action);
    }
}

void Queue::Push(ActionBasket **actions)
{
    if (actions)
    {
        for (int i=0; i<sizeof(actions)/sizeof(ActionBasket*); i++)
        {
            Push(actions[i]);
        }
    }
}

ActionNode* Queue::Pop()
{
    float max = -1;
    ActionBasket* selection = nullptr;
    for (std::list<ActionBasket*>::iterator iter = actions.begin(); iter != actions.end(); iter++)
    {
        ActionBasket* basket = *iter;
        if (basket->getRelevance() > max)
        {
            max = basket->getRelevance();
            selection = basket;
        }
    }
    if (selection != nullptr)
    {
        ActionNode* action = selection->getAction();
        actions.remove(selection);
        delete selection;
        return action;
    }
    return nullptr;
}

ActionBasket* Queue::Peek()
{
    float max = -1;
    ActionBasket* selection = nullptr;
    for (std::list<ActionBasket*>::iterator iter = actions.begin(); iter != actions.end(); iter++)
    {
        ActionBasket* basket = *iter;
        if (basket->getRelevance() > max)
        {
            max = basket->getRelevance();
            selection = basket;
        }
    }
    return selection;
}

int Queue::Size()
{
    return actions.size();
}
