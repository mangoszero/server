#pragma once

namespace ai
{
    class CastStealthedOpeningAction : public CastMeleeSpellAction
    {
    public:
        CastStealthedOpeningAction(PlayerbotAI* ai, string name) : CastMeleeSpellAction(ai, name) {}

        virtual bool isUseful()
        {
            return CastMeleeSpellAction::isUseful() && ai->HasAura("stealth", ai->GetBot());
        }
    };

    class CastSapAction : public CastStealthedOpeningAction
    {
    public:
        CastSapAction(PlayerbotAI* ai) : CastStealthedOpeningAction(ai, "sap") {}

        virtual bool Execute(Event event)
        {
            bool result = CastStealthedOpeningAction::Execute(event);
            if (result)
            {
                Unit* sapTarget = GetTarget();
                if (sapTarget)
                {
                    sapTarget->DeleteThreatList();
                    sapTarget->CombatStop();
                }
                ai->GetBot()->ClearInCombat();
            }
            return result;
        }
    };

    class CastGarroteAction : public CastStealthedOpeningAction
    {
    public:
        CastGarroteAction(PlayerbotAI* ai) : CastStealthedOpeningAction(ai, "garrote") {}

        virtual bool isUseful()
        {
            return CastStealthedOpeningAction::isUseful() && AI_VALUE2(bool, "behind", "current target");
        }
    };

    class CastCheapShotAction : public CastStealthedOpeningAction
    {
    public:
        CastCheapShotAction(PlayerbotAI* ai) : CastStealthedOpeningAction(ai, "cheap shot") {}
    };

}
