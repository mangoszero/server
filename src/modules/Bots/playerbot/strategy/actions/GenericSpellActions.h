#pragma once

#include "../Action.h"
#include "../AiObjectContext.h"
#include "../../PlayerbotAIConfig.h"

#define BEGIN_SPELL_ACTION(clazz, name) \
class clazz : public CastSpellAction \
{ \
public: \
    clazz(PlayerbotAI* ai) : CastSpellAction(ai, name) {} \

#define END_SPELL_ACTION() \
};

#define BEGIN_DEBUFF_ACTION(clazz, name) \
class clazz : public CastDebuffSpellAction \
{ \
public: \
    clazz(PlayerbotAI* ai) : CastDebuffSpellAction(ai, name) {} \

#define BEGIN_RANGED_SPELL_ACTION(clazz, name) \
class clazz : public CastSpellAction \
{ \
public: \
    clazz(PlayerbotAI* ai) : CastSpellAction(ai, name) {} \

#define BEGIN_MELEE_SPELL_ACTION(clazz, name) \
class clazz : public CastMeleeSpellAction \
{ \
public: \
    clazz(PlayerbotAI* ai) : CastMeleeSpellAction(ai, name) {} \

#define END_RANGED_SPELL_ACTION() \
};

#define BEGIN_BUFF_ON_PARTY_ACTION(clazz, name) \
class clazz : public BuffOnPartyAction \
{ \
public: \
    clazz(PlayerbotAI* ai) : BuffOnPartyAction(ai, name) {}

namespace ai
{
    class CastSpellAction : public Action
    {
        public:
            CastSpellAction(PlayerbotAI* ai, string spell) : Action(ai, spell),
                range(sPlayerbotAIConfig.spellDistance),
                rangeFollowsSpell(true),
                lastRangeSpellId(0)
            {
                this->spell = spell;
                // Clamp range to the actual spell's DBC range. NamedObjectContext
                // caches this object for the bot's lifetime (re-randomisation
                // rebuilds engines and strategies but not actions), so the value
                // computed here goes stale when the bot later learns the spell or a
                // rank change resolves to a different id; UpdateRange() re-clamps
                // whenever the resolved id changes.
                lastRangeSpellId = AI_VALUE2(uint32, "spell id", spell);
                range = AI_VALUE2(float, "spell range", spell);
            }

            virtual string GetTargetName()
            {
                return "current target";
            }

            virtual bool Execute(Event event);
            virtual bool isPossible();
            virtual bool isUseful();
            virtual NextAction** getImpossiblePrerequisites();
            virtual ActionThreatType getThreatType()
            {
                return ACTION_THREAT_SINGLE;
            }

            virtual NextAction** getPrerequisites()
            {
                // There used to be an out-of-range branch here that pushed
                // "reach spell" and set the "reach spell distance" value. It was
                // DEAD CODE: the Engine consults prerequisites only after
                // isPossible() has passed, and isPossible() fails first on
                // distance > range, so the branch could never run -- and
                // "reach spell distance" was written but read by nothing. Do not
                // reintroduce either; out-of-range recovery lives in
                // getImpossiblePrerequisites().
                if (range > ATTACK_DISTANCE)
                {
                    return Action::getPrerequisites();
                }
                // NOT dead, unlike the branch above: a melee-policy action passes
                // isPossible() once inside ATTACK_DISTANCE, and this keeps the bot
                // closing to contact.
                return NextAction::merge( NextAction::array(0, new NextAction("reach melee"), NULL), Action::getPrerequisites());
            }

        protected:
            /// Pin a deliberate policy range (ATTACK_DISTANCE, spellDistance) and
            /// opt out of UpdateRange()'s DBC re-clamping.
            void UseFixedRange(float value)
            {
                range = value;
                rangeFollowsSpell = false;
            }

            void UpdateRange(uint32 spellId);

        protected:
            string spell;
            float range;
            bool rangeFollowsSpell;
            uint32 lastRangeSpellId;
    };

    //---------------------------------------------------------------------------------------------------------------------
    class CastAuraSpellAction : public CastSpellAction
    {
        public:
            CastAuraSpellAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell) {}

            virtual bool isUseful();
    };

    //---------------------------------------------------------------------------------------------------------------------
    class CastMeleeSpellAction : public CastSpellAction
    {
        public:
            CastMeleeSpellAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell)
            {
                UseFixedRange(ATTACK_DISTANCE);
            }
    };

    //---------------------------------------------------------------------------------------------------------------------
    class CastDebuffSpellAction : public CastAuraSpellAction
    {
        public:
            CastDebuffSpellAction(PlayerbotAI* ai, string spell) : CastAuraSpellAction(ai, spell) {}
            virtual bool isUseful();
    };

    class CastDebuffSpellOnAttackerAction : public CastAuraSpellAction
    {
        public:
            CastDebuffSpellOnAttackerAction(PlayerbotAI* ai, string spell) : CastAuraSpellAction(ai, spell) {}
            Value<Unit*>* GetTargetValue()
            {
                return context->GetValue<Unit*>("attacker without aura", spell);
            }
            virtual string getName()
            {
                return spell + " on attacker";
            }

            virtual ActionThreatType getThreatType()
            {
                return ACTION_THREAT_AOE;
            }
    };

    class CastBuffSpellAction : public CastAuraSpellAction
    {
        public:
            CastBuffSpellAction(PlayerbotAI* ai, string spell) : CastAuraSpellAction(ai, spell)
            {
                UseFixedRange(sPlayerbotAIConfig.spellDistance);
            }

            virtual string GetTargetName()
            {
                return "self target";
            }
    };

    class CastEnchantItemAction : public CastSpellAction
    {
        public:
            CastEnchantItemAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell)
            {
                UseFixedRange(sPlayerbotAIConfig.spellDistance);
            }

            virtual bool isUseful();
            virtual string GetTargetName()
            {
                return "self target";
            }
    };

    //---------------------------------------------------------------------------------------------------------------------

    class CastHealingSpellAction : public CastAuraSpellAction
    {
        public:
            CastHealingSpellAction(PlayerbotAI* ai, string spell, uint8 estAmount = 15.0f) : CastAuraSpellAction(ai, spell)
            {
                this->estAmount = estAmount;
                UseFixedRange(sPlayerbotAIConfig.spellDistance);
            }
            virtual string GetTargetName()
            {
                return "self target";
            }

            virtual bool isUseful();
            virtual ActionThreatType getThreatType()
            {
                return ACTION_THREAT_AOE;
            }

        protected:
            uint8 estAmount;
    };

    class CastAoeHealSpellAction : public CastHealingSpellAction
    {
        public:
            CastAoeHealSpellAction(PlayerbotAI* ai, string spell, uint8 estAmount = 15.0f) : CastHealingSpellAction(ai, spell, estAmount) {}
            virtual string GetTargetName()
            {
                return "party member to heal";
            }

            virtual bool isUseful();
    };

    class CastCureSpellAction : public CastSpellAction
    {
        public:
            CastCureSpellAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell)
            {
                UseFixedRange(sPlayerbotAIConfig.spellDistance);
            }

            virtual string GetTargetName()
            {
                return "self target";
            }
    };

    class PartyMemberActionNameSupport {
        public:
            PartyMemberActionNameSupport(string spell)
            {
                name = string(spell) + " on party";
            }

            virtual string getName()
            {
                return name;
            }

        private:
            string name;
    };

    class HealPartyMemberAction : public CastHealingSpellAction, public PartyMemberActionNameSupport
    {
        public:
            HealPartyMemberAction(PlayerbotAI* ai, string spell, uint8 estAmount = 15.0f) :
            CastHealingSpellAction(ai, spell, estAmount), PartyMemberActionNameSupport(spell) {}

            virtual string GetTargetName()
            {
                return "party member to heal";
            }

            virtual string getName()
            {
                return PartyMemberActionNameSupport::getName();
            }
    };

    class ResurrectPartyMemberAction : public CastSpellAction
    {
        public:
            ResurrectPartyMemberAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell) {}

            virtual string GetTargetName()
            {
                return "party member to resurrect";
            }

    };
    //---------------------------------------------------------------------------------------------------------------------

    class CurePartyMemberAction : public CastSpellAction, public PartyMemberActionNameSupport
    {
        public:
            CurePartyMemberAction(PlayerbotAI* ai, string spell, uint32 dispelType) :
            CastSpellAction(ai, spell), PartyMemberActionNameSupport(spell)
            {
                this->dispelType = dispelType;
            }

            virtual Value<Unit*>* GetTargetValue();
            virtual string getName()
            {
                return PartyMemberActionNameSupport::getName();
            }

        protected:
            uint32 dispelType;
    };

    //---------------------------------------------------------------------------------------------------------------------

    class BuffOnPartyAction : public CastBuffSpellAction, public PartyMemberActionNameSupport
    {
        public:
            BuffOnPartyAction(PlayerbotAI* ai, string spell) :
            CastBuffSpellAction(ai, spell), PartyMemberActionNameSupport(spell) {}
        public:
            virtual Value<Unit*>* GetTargetValue();
            virtual string getName()
            {
                return PartyMemberActionNameSupport::getName();
            }
    };

    //---------------------------------------------------------------------------------------------------------------------


    class CastLifeBloodAction : public CastHealingSpellAction
    {
        public:
            CastLifeBloodAction(PlayerbotAI* ai) : CastHealingSpellAction(ai, "lifeblood") {}
    };

    class CastGiftOfTheNaaruAction : public CastHealingSpellAction
    {
        public:
            CastGiftOfTheNaaruAction(PlayerbotAI* ai) : CastHealingSpellAction(ai, "gift of the naaru") {}
    };

    class CastArcaneTorrentAction : public CastBuffSpellAction
    {
        public:
            CastArcaneTorrentAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "arcane torrent") {}
    };

    class CastSpellOnEnemyHealerAction : public CastSpellAction
    {
        public:
            CastSpellOnEnemyHealerAction(PlayerbotAI* ai, string spell) : CastSpellAction(ai, spell) {}
            Value<Unit*>* GetTargetValue()
            {
                return context->GetValue<Unit*>("enemy healer target", spell);
            }

            virtual string getName()
            {
                return spell + " on enemy healer";
            }
    };
}
