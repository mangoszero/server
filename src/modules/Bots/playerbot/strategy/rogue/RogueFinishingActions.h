#pragma once

namespace ai
{
    class CastFinishingMoveAction : public CastMeleeSpellAction
    {
    public:
        CastFinishingMoveAction(PlayerbotAI* ai, string name) : CastMeleeSpellAction(ai, name) {}

        virtual bool isUseful()
        {
            return CastMeleeSpellAction::isUseful() && AI_VALUE2(uint8, "combo", "current target") >= 1;
        }
    };

    class CastEviscerateAction : public CastFinishingMoveAction
    {
    public:
        CastEviscerateAction(PlayerbotAI* ai) : CastFinishingMoveAction(ai, "eviscerate") {}
    };

    class CastSliceAndDiceAction : public CastFinishingMoveAction
    {
    public:
        CastSliceAndDiceAction(PlayerbotAI* ai) : CastFinishingMoveAction(ai, "slice and dice") {}
    };

    class CastExposeArmorAction : public CastFinishingMoveAction
    {
    public:
        CastExposeArmorAction(PlayerbotAI* ai) : CastFinishingMoveAction(ai, "expose armor") {}
    };

    class CastRuptureAction : public CastFinishingMoveAction
    {
    public:
        CastRuptureAction(PlayerbotAI* ai) : CastFinishingMoveAction(ai, "rupture") {}
    };

    class CastKidneyShotAction : public CastFinishingMoveAction
    {
    public:
        CastKidneyShotAction(PlayerbotAI* ai) : CastFinishingMoveAction(ai, "kidney shot") {}
    };

}
