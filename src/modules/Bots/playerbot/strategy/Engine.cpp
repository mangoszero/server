#include "../../botpch.h"
#include "../playerbot.h"

#include "Engine.h"
#include "../PlayerbotAIConfig.h"

using namespace ai;
using namespace std;

// Constructor for the Engine class
Engine::Engine(PlayerbotAI* ai, AiObjectContext *factory) : PlayerbotAIAware(ai), aiObjectContext(factory)
{
    lastRelevance = 0.0f;
    testMode = false;
}

// Executes actions before the main action
bool ActionExecutionListeners::Before(Action* action, Event event)
{
    bool result = true;
    for (auto listener : listeners)
    {
        result &= listener->Before(action, event);
    }
    return result;
}

// Executes actions after the main action
void ActionExecutionListeners::After(Action* action, bool executed, Event event)
{
    for (auto listener : listeners)
    {
        listener->After(action, executed, event);
    }
}

// Overrides the result of the action execution
bool ActionExecutionListeners::OverrideResult(Action* action, bool executed, Event event)
{
    bool result = executed;
    for (auto listener : listeners)
    {
        result = listener->OverrideResult(action, result, event);
    }
    return result;
}

// Checks if the action execution is allowed
bool ActionExecutionListeners::AllowExecution(Action* action, Event event)
{
    bool result = true;
    for (auto listener : listeners)
    {
        result &= listener->AllowExecution(action, event);
    }
    return result;
}

// Destructor for ActionExecutionListeners
ActionExecutionListeners::~ActionExecutionListeners()
{
    for (auto listener : listeners)
    {
        delete listener;
    }
    listeners.clear();
}

// Destructor for the Engine class
Engine::~Engine(void)
{
    Reset();
    strategies.clear();
}

// Resets the engine by clearing the action queue, triggers, and multipliers
void Engine::Reset()
{
    ActionNode* action = NULL;
    do
    {
        action = queue.Pop();
        delete action;
    } while (action != NULL);

    for (list<TriggerNode*>::iterator i = triggers.begin(); i != triggers.end(); i++)
    {
        TriggerNode* trigger = *i;
        delete trigger;
    }
    triggers.clear();

    for (list<Multiplier*>::iterator i = multipliers.begin(); i != multipliers.end(); i++)
    {
        Multiplier* multiplier = *i;
        delete multiplier;
    }
    multipliers.clear();
}

// Initializes the engine by resetting it and initializing strategies
void Engine::Init()
{
    Reset();

    for (map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); i++)
    {
        Strategy* strategy = i->second;
        strategy->InitMultipliers(multipliers);
        strategy->InitTriggers(triggers);
        Event emptyEvent;
        MultiplyAndPush(strategy->getDefaultActions(), 0.0f, false, emptyEvent);
    }

    if (testMode)
    {
        FILE* file = fopen("test.log", "w");
        fprintf(file, "\n");
        fclose(file);
    }
}

// Executes the next action in the queue
bool Engine::DoNextAction(Unit* unit, int depth)
{
    LogAction("--- AI Tick ---");
    if (sPlayerbotAIConfig.logValuesPerTick)
    {
        LogValues();
    }

    bool actionExecuted = false;
    ActionBasket* basket = NULL;

    time_t currentTime = time(0);
    aiObjectContext->Update();
    ProcessTriggers();

    int iterations = 0;
    int iterationsPerTick = queue.Size() * sPlayerbotAIConfig.iterationsPerTick;
    do {
        basket = queue.Peek();
        if (basket)
        {
            if (++iterations > iterationsPerTick)
            {
                break;
            }

            float relevance = basket->getRelevance(); // just for reference
            bool skipPrerequisites = basket->isSkipPrerequisites();
            Event event = basket->getEvent();
            // NOTE: queue.Pop() deletes basket
            ActionNode* actionNode = queue.Pop();
            Action* action = InitializeAction(actionNode);

            if (!action)
            {
                LogAction("A:%s - UNKNOWN", actionNode->getName().c_str());
                MultiplyAndPush(actionNode->getAlternatives(), relevance + 0.03, false, event);
            }
            else if (action->isUseful())
            {
                for (list<Multiplier*>::iterator i = multipliers.begin(); i!= multipliers.end(); i++)
                {
                    Multiplier* multiplier = *i;
                    relevance *= multiplier->GetValue(action);
                    if (!relevance)
                    {
                        LogAction("Multiplier %s made action %s useless", multiplier->getName().c_str(), action->getName().c_str());
                        break;
                    }
                }

                if (action->isPossible() && relevance)
                {
                    if ((!skipPrerequisites || lastRelevance-relevance > 0.04) &&
                            MultiplyAndPush(actionNode->getPrerequisites(), relevance + 0.02, false, event))
                    {
                        PushAgain(actionNode, relevance + 0.01, event);
                        continue;
                    }

                    actionExecuted = ListenAndExecute(action, event);

                    if (actionExecuted)
                    {
                        LogAction("A:%s - OK", action->getName().c_str());
                        MultiplyAndPush(actionNode->getContinuers(), 0, false, event);
                        lastRelevance = relevance;
                        delete actionNode;
                        break;
                    }
                    else
                    {
                        LogAction("A:%s - FAILED", action->getName().c_str());
                        MultiplyAndPush(actionNode->getAlternatives(), relevance + 0.03, false, event);
                    }
                }
                else
                {
                    LogAction("A:%s - IMPOSSIBLE", action->getName().c_str());
                    MultiplyAndPush(actionNode->getAlternatives(), relevance + 0.03, false, event);
                }
            }
            else
            {
                lastRelevance = relevance;
                LogAction("A:%s - USELESS", action->getName().c_str());
            }
            delete actionNode;
        }
    }
    while (basket);

    if (!basket)
    {
        lastRelevance = 0.0f;
        PushDefaultActions();
        if (queue.Peek() && depth < 2)
        {
            return DoNextAction(unit, depth + 1);
        }
    }

    if (time(0) - currentTime > 1) {
    {
        LogAction("too long execution");
    }
    }

    if (!actionExecuted)
    {
        LogAction("no actions executed");
    }

    return actionExecuted;
}

// Creates an action node based on the action name
ActionNode* Engine::CreateActionNode(string name)
{
    for (map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); i++)
    {
        Strategy* strategy = i->second;
        ActionNode* node = strategy->GetAction(name);
        if (node)
        {
            return node;
        }
    }
    return new ActionNode (name,
        /*P*/ NULL,
        /*A*/ NULL,
        /*C*/ NULL);
}

// Multiplies the relevance of actions and pushes them to the queue
bool Engine::MultiplyAndPush(NextAction** actions, float forceRelevance, bool skipPrerequisites, Event event)
{
    bool pushed = false;
    if (actions)
    {
        for (int j=0; j<10; j++) // TODO: remove 10
        {
            NextAction* nextAction = actions[j];
            if (nextAction)
            {
                ActionNode* action = CreateActionNode(nextAction->getName());
                InitializeAction(action);

                float k = nextAction->getRelevance();
                if (forceRelevance > 0.0f)
                {
                    k = forceRelevance;
                }

                if (k > 0)
                {
                    LogAction("PUSH:%s %f", action->getName().c_str(), k);
                    queue.Push(new ActionBasket(action, k, skipPrerequisites, event));
                    pushed = true;
                }
                else
                {
                    delete action;
                }

                delete nextAction;
            }
            else
            {
                break;
            }
        }
        delete [] actions;
    }
    return pushed;
}

// Executes an action based on its name
ActionResult Engine::ExecuteAction(string &name)
{
    bool result = false;

    ActionNode *actionNode = CreateActionNode(name);
    if (!actionNode)
    {
        return ACTION_RESULT_UNKNOWN;
    }

    Action* action = InitializeAction(actionNode);
    if (!action)
    {
        return ACTION_RESULT_UNKNOWN;
    }

    if (!action->isPossible())
    {
        delete actionNode;
        return ACTION_RESULT_IMPOSSIBLE;
    }

    if (!action->isUseful())
    {
        delete actionNode;
        return ACTION_RESULT_USELESS;
    }

    action->MakeVerbose();
    Event emptyEvent;
    result = ListenAndExecute(action, emptyEvent);
    MultiplyAndPush(action->getContinuers(), 0.0f, false, emptyEvent);
    delete actionNode;
    return result ? ACTION_RESULT_OK : ACTION_RESULT_FAILED;
}

// Adds a strategy to the engine
void Engine::addStrategy(string name)
{
    removeStrategy(name);

    Strategy* strategy = aiObjectContext->GetStrategy(name);
    if (strategy)
    {
        set<string> siblings = aiObjectContext->GetSiblingStrategy(name);
        for (set<string>::iterator i = siblings.begin(); i != siblings.end(); i++)
        {
            removeStrategy(*i);
        }

        LogAction("S:+%s", strategy->getName().c_str());
        strategies[strategy->getName()] = strategy;
    }
    Init();
}

// Adds multiple strategies to the engine
void Engine::addStrategies(string first, ...)
{
    addStrategy(first);

    va_list vl;
    va_start(vl, first);

    const char* cur;
    do
    {
        cur = va_arg(vl, const char*);
        if (cur)
        {
            addStrategy(cur);
        }
    }
    while (cur);

    va_end(vl);
}

// Removes a strategy from the engine
bool Engine::removeStrategy(string name)
{
    map<string, Strategy*>::iterator i = strategies.find(name);
    if (i == strategies.end())
    {
        return false;
    }

    LogAction("S:-%s", name.c_str());
    strategies.erase(i);
    Init();
    return true;
}

// Removes all strategies from the engine
void Engine::removeAllStrategies()
{
    strategies.clear();
    Init();
}

// Toggles a strategy in the engine
void Engine::toggleStrategy(string name)
{
    if (!removeStrategy(name))
    {
        addStrategy(name);
    }
}

// Checks if the engine has a specific strategy
bool Engine::HasStrategy(string name)
{
    return strategies.find(name) != strategies.end();
}

// Processes triggers and fires events
void Engine::ProcessTriggers()
{
    for (list<TriggerNode*>::iterator i = triggers.begin(); i != triggers.end(); i++)
    {
        TriggerNode* node = *i;
        if (!node)
        {
            continue;
        }

        Trigger* trigger = node->getTrigger();
        if (!trigger)
        {
            trigger = aiObjectContext->GetTrigger(node->getName());
            node->setTrigger(trigger);
        }

        if (!trigger)
        {
            continue;
        }

        if (testMode || trigger->needCheck())
        {
            Event event = trigger->Check();
            if (!event)
            {
                continue;
            }

            LogAction("T:%s", trigger->getName().c_str());
            MultiplyAndPush(node->getHandlers(), 0.0f, false, event);
        }
    }

    for (list<TriggerNode*>::iterator i = triggers.begin(); i != triggers.end(); i++)
    {
        Trigger* trigger = (*i)->getTrigger();
        if (trigger) trigger->Reset();
    }
}

// Pushes default actions to the queue
void Engine::PushDefaultActions()
{
    for (map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); i++)
    {
        Strategy* strategy = i->second;
        Event emptyEvent;
        MultiplyAndPush(strategy->getDefaultActions(), 0.0f, false, emptyEvent);
    }
}

// Lists all strategies in the engine
string Engine::ListStrategies()
{
    string s = "Strategies: ";

    if (strategies.empty())
    {
        return s;
    }

    for (map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); i++)
    {
        s.append(i->first);
        s.append(", ");
    }
    return s.substr(0, s.length() - 2);
}

// Pushes an action node to the queue again
void Engine::PushAgain(ActionNode* actionNode, float relevance, Event event)
{
    NextAction** nextAction = new NextAction*[2];
    nextAction[0] = new NextAction(actionNode->getName(), relevance);
    nextAction[1] = NULL;
    MultiplyAndPush(nextAction, relevance, true, event);
    delete actionNode;
}

// Checks if the engine contains a specific strategy type
bool Engine::ContainsStrategy(StrategyType type)
{
    for (map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); i++)
    {
        Strategy* strategy = i->second;
        if (strategy->GetType() & type)
        {
            return true;
        }
    }
    return false;
}

// Initializes an action based on the action node
Action* Engine::InitializeAction(ActionNode* actionNode)
{
    Action* action = actionNode->getAction();
    if (!action)
    {
        action = aiObjectContext->GetAction(actionNode->getName());
        actionNode->setAction(action);
    }
    return action;
}

// Listens and executes an action
bool Engine::ListenAndExecute(Action* action, Event event)
{
    bool actionExecuted = false;

    if (actionExecutionListeners.Before(action, event))
    {
        actionExecuted = actionExecutionListeners.AllowExecution(action, event) ? action->Execute(event) : true;
    }

    actionExecuted = actionExecutionListeners.OverrideResult(action, actionExecuted, event);
    actionExecutionListeners.After(action, actionExecuted, event);

    return actionExecuted;
}

/**
 * Get list of strategy names
 * @return list<string> List of strategy names
 */
list<string> Engine::GetStrategies()
{
    list<string> result;
    for (std::map<string, Strategy*>::iterator i = strategies.begin(); i != strategies.end(); ++i)
    {
        result.push_back(i->first);
    }
    return result;
}

// Logs an action
void Engine::LogAction(const char* format, ...)
{
    char buf[1024];

    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    lastAction = buf;

    if (testMode)
    {
        FILE* file = fopen("test.log", "a");
        fprintf(file, buf);
        fprintf(file, "\n");
        fclose(file);
    }
    else
    {
        Player* bot = ai->GetBot();
        if (sPlayerbotAIConfig.logInGroupOnly && !bot->GetGroup())
        {
            return;
        }

        sLog.outDebug("%s %s", bot->GetName(), buf);
    }
}

// Changes the strategy based on the provided names
void Engine::ChangeStrategy(string &names)
{
    vector<string> splitted = split(names, ',');
    for (vector<string>::iterator i = splitted.begin(); i != splitted.end(); i++)
    {
        const char* name = i->c_str();
        switch (name[0])
        {
        case '+':
            addStrategy(name+1);
            break;
        case '-':
            removeStrategy(name+1);
            break;
        case '~':
            toggleStrategy(name+1);
            break;
        case '?':
            ai->TellMaster(ListStrategies());
            break;
        }
    }
}

// Logs the values of the AI context
void Engine::LogValues()
{
    if (testMode)
    {
        return;
    }

    Player* bot = ai->GetBot();
    if (sPlayerbotAIConfig.logInGroupOnly && !bot->GetGroup())
    {
        return;
    }

    string text = ai->GetAiObjectContext()->FormatValues();
    sLog.outDebug( "Values for %s: %s", bot->GetName(), text.c_str());
}
