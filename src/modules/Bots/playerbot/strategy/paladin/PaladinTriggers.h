#pragma once
#include "../triggers/GenericTriggers.h"
#include "../triggers/CureTriggers.h"

namespace ai
{
    BUFF_TRIGGER(HolyShieldTrigger, "holy shield", "holy shield")
    BUFF_TRIGGER(RighteousFuryTrigger, "righteous fury", "righteous fury")

    BUFF_TRIGGER(RetributionAuraTrigger, "retribution aura", "retribution aura")

    class CrusaderAuraTrigger : public BuffTrigger
    {
        public:
            CrusaderAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "crusader aura") {}
            virtual bool IsActive();
    };

    class SealTrigger : public BuffTrigger
    {
        public:
            SealTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "seal of justice") {}
            virtual bool IsActive();
    };

    DEBUFF_TRIGGER(JudgementOfLightTrigger, "judgement of light", "judgement of light")
    DEBUFF_TRIGGER(JudgementOfWisdomTrigger, "judgement of wisdom", "judgement of wisdom")

    /// Every blessing a target may already be carrying, greater ranks included.
    ///
    /// The list has to cover what CastBlessings itself casts, or the bot cannot see the aura
    /// it just applied and casts it again. "greater blessing of wisdom" was missing while
    /// being the primary choice for priest, mage, warlock, shaman, druid AND paladin -- every
    /// caster in the game -- so blessing any of them looped until the paladin was out of
    /// mana. Caught in a packet capture: 18 casts of Greater Blessing of Wisdom in thirteen
    /// seconds, ending in Drink.
    ///
    /// The remaining greater ranks are listed for a second reason: they are all real 1.12
    /// spells, and a target already blessed by a player or another paladin should be left
    /// alone rather than overwritten with something worse.
    ///
    /// Freedom, Protection and Sacrifice are here for a THIRD reason, and it is the sharpest
    /// of the three. GetSpellSpecific classifies a paladin spell as SPELL_BLESSING through
    /// IsFitToFamilyMask(0x0000000010000100), and that test is `Flags & familyFlags` -- any
    /// bit, not all -- so Freedom (0x10000010), Protection (0x10000080) and Sacrifice
    /// (0x10000000) all qualify alongside Might (0x10000002). SPELL_BLESSING is in
    /// IsSingleFromSpellSpecificPerTargetPerCaster, so one blessing per caster per target:
    /// they STRIP each other. BlessingOfFreedomTrigger fires at ACTION_EMERGENCY whenever the
    /// paladin is snared, so Freedom would remove the maintained Might, this guard would not
    /// see Freedom, Might would be recast and remove the Freedom the bot paid for one GCD
    /// ago, and a persistent snare would alternate the pair every global cooldown.
    inline bool HasAnyBlessing(PlayerbotAI* ai, Unit* target)
    {
        for (const char* b : {"blessing of kings", "blessing of might", "blessing of sanctuary",
                    "blessing of wisdom", "blessing of salvation", "blessing of light",
                    "greater blessing of kings", "greater blessing of might",
                    "greater blessing of wisdom", "greater blessing of salvation",
                    "greater blessing of sanctuary", "greater blessing of light",
                    "blessing of freedom", "blessing of protection",
                    "blessing of sacrifice"})
        {
            if (ai->HasAura(b, target))
            {
                return true;
            }
        }
        return false;
    }

    class BlessingTrigger : public Trigger
    {
        public:
            BlessingTrigger(PlayerbotAI* ai) : Trigger(ai, "blessing") {}
            virtual bool IsActive()
            {
                Unit* target = GetTarget();
                return target && !HasAnyBlessing(ai, target);
            }
    };

    class AuraTrigger : public BuffTrigger
    {
        public:
            AuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "devotion aura") {}
            virtual bool IsActive();
    };

    class HammerOfJusticeInterruptSpellTrigger : public InterruptSpellTrigger
    {
        public:
            HammerOfJusticeInterruptSpellTrigger(PlayerbotAI* ai) : InterruptSpellTrigger(ai, "hammer of justice") {}
    };

    class HammerOfJusticeSnareTrigger : public SnareTargetTrigger
    {
        public:
            HammerOfJusticeSnareTrigger(PlayerbotAI* ai) : SnareTargetTrigger(ai, "hammer of justice") {}
    };

    class ArtOfWarTrigger : public HasAuraTrigger
    {
        public:
            ArtOfWarTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "the art of war") {}
    };

    class ShadowResistanceAuraTrigger : public BuffTrigger
    {
        public:
            ShadowResistanceAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "shadow resistance aura") {}
    };

    class FrostResistanceAuraTrigger : public BuffTrigger
    {
        public:
            FrostResistanceAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "frost resistance aura") {}
    };

    class FireResistanceAuraTrigger : public BuffTrigger
    {
        public:
            FireResistanceAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "fire resistance aura") {}
    };

    class DevotionAuraTrigger : public BuffTrigger
    {
        public:
            DevotionAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "devotion aura") {}
    };

    class CleanseCureDiseaseTrigger : public NeedCureTrigger
    {
        public:
            CleanseCureDiseaseTrigger(PlayerbotAI* ai) : NeedCureTrigger(ai, "cleanse", DISPEL_DISEASE) {}
    };

    class CleanseCurePartyMemberDiseaseTrigger : public PartyMemberNeedCureTrigger
    {
        public:
            CleanseCurePartyMemberDiseaseTrigger(PlayerbotAI* ai) : PartyMemberNeedCureTrigger(ai, "cleanse", DISPEL_DISEASE) {}
    };

    class CleanseCurePoisonTrigger : public NeedCureTrigger
    {
        public:
            CleanseCurePoisonTrigger(PlayerbotAI* ai) : NeedCureTrigger(ai, "cleanse", DISPEL_POISON) {}
    };

    class CleanseCurePartyMemberPoisonTrigger : public PartyMemberNeedCureTrigger
    {
        public:
            CleanseCurePartyMemberPoisonTrigger(PlayerbotAI* ai) : PartyMemberNeedCureTrigger(ai, "cleanse", DISPEL_POISON) {}
    };

    class CleanseCureMagicTrigger : public NeedCureTrigger
    {
        public:
            CleanseCureMagicTrigger(PlayerbotAI* ai) : NeedCureTrigger(ai, "cleanse", DISPEL_MAGIC) {}
    };

    class CleanseCurePartyMemberMagicTrigger : public PartyMemberNeedCureTrigger
    {
        public:
            CleanseCurePartyMemberMagicTrigger(PlayerbotAI* ai) : PartyMemberNeedCureTrigger(ai, "cleanse", DISPEL_MAGIC) {}
    };

    class HammerOfJusticeEnemyHealerTrigger : public InterruptEnemyHealerTrigger
    {
        public:
            HammerOfJusticeEnemyHealerTrigger(PlayerbotAI* ai) : InterruptEnemyHealerTrigger(ai, "hammer of justice") {}
    };

    class HolyWrathTrigger : public SpellTrigger
    {
        public:
            HolyWrathTrigger(PlayerbotAI* ai) : SpellTrigger(ai, "holy wrath") {}
            virtual bool IsActive();
    };

    class ExorcismTrigger : public SpellTrigger
    {
        public:
            ExorcismTrigger(PlayerbotAI* ai) : SpellTrigger(ai, "exorcism") {}
            virtual bool IsActive();
    };

    // Fires when the bot itself is rooted — cast Blessing of Freedom on self.
    class BlessingOfFreedomTrigger : public Trigger
    {
        public:
            BlessingOfFreedomTrigger(PlayerbotAI* ai) : Trigger(ai, "blessing of freedom") {}
            virtual bool IsActive();
    };
}
