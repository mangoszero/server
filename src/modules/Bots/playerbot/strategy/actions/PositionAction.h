#pragma once

#include "../Action.h"
#include "MovementActions.h"

namespace ai
{
    class PositionAction : public Action
    {
        public:
            PositionAction(PlayerbotAI* ai) : Action(ai, "position")
            {}

            virtual bool Execute(Event event);

    };

    class MoveToPositionAction : public MovementAction
    {
        public:
            MoveToPositionAction(PlayerbotAI* ai, string qualifier) : MovementAction(ai, "move to position"), qualifier(qualifier)
            {}

            virtual bool Execute(Event event);

        protected:
            string qualifier;
    };

    class GuardAction : public MoveToPositionAction
    {
        public:
            GuardAction(PlayerbotAI* ai) : MoveToPositionAction(ai, "guard")
            {}
    };

    class GotoAction : public MovementAction
    {
        public:
            GotoAction(PlayerbotAI* ai) : MovementAction(ai, "goto") {}
            virtual bool Execute(Event event);

        private:
            string m_positionName;
            uint32 m_deadline = 0;
            float m_targetX = 0, m_targetY = 0, m_targetZ = 0;
            uint32 m_targetMapId = 0;
    };
}
