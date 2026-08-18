#pragma once

#include "../Action.h"
#include "AttackAction.h"

namespace ai
{
    class DpsAssistAction : public AttackAction
    {
        public:
            DpsAssistAction(PlayerbotAI* ai) : AttackAction(ai, "dps assist") {}

            virtual string GetTargetName()
            {
                return "dps target";
            }
    };

    class TankAssistAction : public AttackAction
    {
        public:
            TankAssistAction(PlayerbotAI* ai) : AttackAction(ai, "tank assist") {}

            virtual string GetTargetName()
            {
                return "tank target";
            }
    };

    class AttackAnythingAction : public AttackAction
    {
        public:
            AttackAnythingAction(PlayerbotAI* ai) : AttackAction(ai, "attack anything") {}

            virtual string GetTargetName()
            {
                return "grind target";
            }
            virtual bool Execute(Event event)
            {
                return AttackAction::Execute(event);
            }

            virtual bool isUseful()
            {
                return AttackAction::isUseful() && GetTarget() &&
                    (AI_VALUE2(uint8, "health", "self target") > sPlayerbotAIConfig.mediumHealth &&
                    (!AI_VALUE2(uint8, "mana", "self target") || AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.mediumMana)) || AI_VALUE2(bool, "combat", "self target");
            }
            virtual bool isPossible()
            {
                return AttackAction::isPossible() && GetTarget();
            }
    };

    class AttackLeastHpTargetAction : public AttackAction
    {
        public:
            AttackLeastHpTargetAction(PlayerbotAI* ai) : AttackAction(ai, "attack least hp target") {}

            virtual string GetTargetName()
            {
                return "least hp target";
            }
    };

    class AttackTanksTargetAction : public AttackAction
    {
        public:
            AttackTanksTargetAction(PlayerbotAI* ai) : AttackAction(ai, "attack tanks target") {}

            virtual string GetTargetName()
            {
                Player* tank = ai->GetGroupTank(bot);
                if (!tank || !tank->IsAlive())
                {
                    return "least hp target";
                }
                return "dps tanks target";
            }
    };

    class AttackEnemyPlayerAction : public AttackAction
    {
        public:
            AttackEnemyPlayerAction(PlayerbotAI* ai) : AttackAction(ai, "attack enemy player") {}

            virtual string GetTargetName()
            {
                return "enemy player target";
            }
    };

    class AttackRtiTargetAction : public AttackAction
    {
        public:
            AttackRtiTargetAction(PlayerbotAI* ai) : AttackAction(ai, "attack rti target") {}
            virtual string GetTargetName()
            {
                return "rti target";
            }
    };

    class DropTargetAction : public Action
    {
        public:
            DropTargetAction(PlayerbotAI* ai) : Action(ai, "drop target") {}

            virtual bool Execute(Event event)
            {
                // Preserve the raw combat target before the filtered value and selection are
                // cleared. The selection fallback also covers a pet-only surviving attack.
                Unit* victim = bot->getVictim();
                if (!victim && !bot->GetSelectionGuid().IsEmpty())
                {
                    victim = ai->GetUnit(bot->GetSelectionGuid());
                }

                Pet* pet = bot->GetPet();

                // Merely clearing the AI target leaves this bot in victim->getAttackers().
                // That makes Unit::IsVisibleForOrDetect prove its own stealth visibility.
                bot->AttackStop();

                if (pet && victim && pet->getVictim() == victim)
                {
                    // Match PetAI::_stopAttack(): discard the chase generator, idle, then
                    // remove the pet from the victim's attacker set.
                    pet->GetMotionMaster()->Clear(false);
                    pet->GetMotionMaster()->MoveIdle();
                    pet->AttackStop();
                }

                ai->StopMovement();
                context->GetValue<Unit*>("current target")->Set(NULL);
                bot->SetSelectionGuid(ObjectGuid());
                ai->ChangeEngine(BOT_STATE_NON_COMBAT);
                ai->InterruptSpell();
                return true;
            }
    };
}
