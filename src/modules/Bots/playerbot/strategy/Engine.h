#pragma once

#include "Action.h"
#include "Queue.h"
#include "Trigger.h"
#include "Multiplier.h"
#include "AiObjectContext.h"
#include "Strategy.h"

namespace ai
{
    /**
     * @brief Interface for action execution listeners
     */
    class ActionExecutionListener
    {
    public:
        virtual ~ActionExecutionListener() = default; // Add a virtual destructor
        virtual bool Before(Action* action, Event event) = 0;
        virtual bool AllowExecution(Action* action, Event event) = 0;
        virtual void After(Action* action, bool executed, Event event) = 0;
        virtual bool OverrideResult(Action* action, bool executed, Event event) = 0;
    };

    // -----------------------------------------------------------------------------------------------------------------------

    /**
     * @brief Manages a list of action execution listeners
     */
    class ActionExecutionListeners : public ActionExecutionListener
    {
    public:
        virtual ~ActionExecutionListeners();

    // ActionExecutionListener
    public:
        virtual bool Before(Action* action, Event event);
        virtual bool AllowExecution(Action* action, Event event);
        virtual void After(Action* action, bool executed, Event event);
        virtual bool OverrideResult(Action* action, bool executed, Event event);

    public:
        /**
         * @brief Add a listener to the list
         *
         * @param listener The listener to add
         */
        void Add(ActionExecutionListener* listener)
        {
            listeners.push_back(listener);
        }

        /**
         * @brief Remove a listener from the list
         *
         * @param listener The listener to remove
         */
        void Remove(ActionExecutionListener* listener)
        {
            listeners.remove(listener);
        }

    private:
        std::list<ActionExecutionListener*> listeners; /**< List of action execution listeners */
    };

    // -----------------------------------------------------------------------------------------------------------------------

    /**
     * @brief Enumeration for action results
     */
    enum ActionResult
    {
        ACTION_RESULT_UNKNOWN,
        ACTION_RESULT_OK,
        ACTION_RESULT_IMPOSSIBLE,
        ACTION_RESULT_USELESS,
        ACTION_RESULT_FAILED
    };

    /**
     * @brief Engine class for managing AI strategies and actions
     */
    class Engine : public PlayerbotAIAware
    {
    public:
        Engine(PlayerbotAI* ai, AiObjectContext *factory);

        void Init();
        void addStrategy(string name);
        void addStrategies(string first, ...);
        bool removeStrategy(string name);
        bool HasStrategy(string name);
        void removeAllStrategies();
        void toggleStrategy(string name);
        std::string ListStrategies();
        bool ContainsStrategy(StrategyType type);
        void ChangeStrategy(string &names);
        string GetLastAction() { return lastAction; }

    public:
        virtual bool DoNextAction(Unit*, int depth = 0);
        ActionResult ExecuteAction(string &name);

    public:
        /**
         * @brief Add an action execution listener
         *
         * @param listener The listener to add
         */
        void AddActionExecutionListener(ActionExecutionListener* listener)
        {
            actionExecutionListeners.Add(listener);
        }

        /**
         * @brief Remove an action execution listener
         *
         * @param listener The listener to remove
         */
        void removeActionExecutionListener(ActionExecutionListener* listener)
        {
            actionExecutionListeners.Remove(listener);
        }

    public:
        virtual ~Engine(void);

    private:
        bool MultiplyAndPush(NextAction** actions, float forceRelevance, bool skipPrerequisites, Event event);
        void Reset();
        void ProcessTriggers();
        void PushDefaultActions();
        void PushAgain(ActionNode* actionNode, float relevance, Event event);
        ActionNode* CreateActionNode(string name);
        Action* InitializeAction(ActionNode* actionNode);
        bool ListenAndExecute(Action* action, Event event);

    private:
        void LogAction(const char* format, ...);
        void LogValues();

    protected:
        Queue queue; /**< Queue for managing actions */
        std::list<TriggerNode*> triggers; /**< List of triggers */
        std::list<Multiplier*> multipliers; /**< List of multipliers */
        AiObjectContext* aiObjectContext; /**< AI object context */
        std::map<string, Strategy*> strategies; /**< Map of strategies */
        float lastRelevance; /**< Last relevance value */
        std::string lastAction; /**< Last executed action */

    public:
        bool testMode; /**< Flag for test mode */

    private:
        ActionExecutionListeners actionExecutionListeners; /**< Listeners for action execution */
    };
}
