#pragma once
#include "../triggers/GenericTriggers.h"

namespace ai
{
    BUFF_ON_PARTY_TRIGGER(ArcaneIntellectOnPartyTrigger, "arcane intellect", "arcane intellect on party")
    BUFF_TRIGGER(ArcaneIntellectTrigger, "arcane intellect", "arcane intellect")

    class MageArmorTrigger : public BuffTrigger {
    public:
        MageArmorTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "mage armor") {}
        virtual bool IsActive();
    };

    class LivingBombTrigger : public DebuffTrigger {
    public:
        LivingBombTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "living bomb") {}
    };

    class FireballTrigger : public DebuffTrigger {
    public:
        FireballTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "fireball") {}
    };

    class PyroblastTrigger : public DebuffTrigger {
    public:
        PyroblastTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "pyroblast") {}
    };

    class HotStreakTrigger : public HasAuraTrigger {
    public:
        HotStreakTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "hot streak") {}
    };

    class MissileBarrageTrigger : public HasAuraTrigger {
    public:
        MissileBarrageTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "missile barrage") {}
    };

    class ArcaneBlastTrigger : public BuffTrigger {
    public:
        ArcaneBlastTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "arcane blast") {}
    };

    class CounterspellInterruptSpellTrigger : public InterruptSpellTrigger
    {
    public:
        CounterspellInterruptSpellTrigger(PlayerbotAI* ai) : InterruptSpellTrigger(ai, "counterspell") {}
    };

    class CombustionTrigger : public BoostTrigger
    {
    public:
        CombustionTrigger(PlayerbotAI* ai) : BoostTrigger(ai, "combustion") {}
    };

    class IcyVeinsTrigger : public BoostTrigger
    {
    public:
        IcyVeinsTrigger(PlayerbotAI* ai) : BoostTrigger(ai, "icy veins") {}
    };

    class PolymorphTrigger : public HasCcTargetTrigger
    {
    public:
        PolymorphTrigger(PlayerbotAI* ai) : HasCcTargetTrigger(ai, "polymorph") {}
    };

    class RemoveCurseTrigger : public NeedCureTrigger
    {
    public:
        RemoveCurseTrigger(PlayerbotAI* ai) : NeedCureTrigger(ai, "remove curse", DISPEL_CURSE) {}
    };

    class PartyMemberRemoveCurseTrigger : public PartyMemberNeedCureTrigger
    {
    public:
        PartyMemberRemoveCurseTrigger(PlayerbotAI* ai) : PartyMemberNeedCureTrigger(ai, "remove curse", DISPEL_CURSE) {}
    };

    class SpellstealTrigger : public TargetAuraDispelTrigger
    {
    public:
        SpellstealTrigger(PlayerbotAI* ai) : TargetAuraDispelTrigger(ai, "spellsteal", DISPEL_MAGIC) {}
    };

    class CounterspellEnemyHealerTrigger : public InterruptEnemyHealerTrigger
    {
    public:
        CounterspellEnemyHealerTrigger(PlayerbotAI* ai) : InterruptEnemyHealerTrigger(ai, "counterspell") {}
    };

    class NoManaGemTrigger : public ItemCountTrigger
    {
    public:
        NoManaGemTrigger(PlayerbotAI* ai) : ItemCountTrigger(ai, "mana gem", 1) {}
    };

    class PartyMemberNeedsSustenanceTrigger : public Trigger
    {
    public:
        PartyMemberNeedsSustenanceTrigger(PlayerbotAI* ai, string name, uint32 spellCategory)
            : Trigger(ai, name, 5), m_spellCategory(spellCategory),
              m_lastScanTime(0), m_lastResult(false) {}
        virtual bool IsActive();
    private:
        static const uint32 IDLE_SCAN_MS   = 30000;
        static const uint32 ACTIVE_SCAN_MS =  2500;
        uint32 m_spellCategory;
        uint32 m_lastScanTime;
        bool   m_lastResult;
    };

    class PartyMemberNeedsFoodTrigger : public PartyMemberNeedsSustenanceTrigger
    {
    public:
        PartyMemberNeedsFoodTrigger(PlayerbotAI* ai)
            : PartyMemberNeedsSustenanceTrigger(ai, "party member needs food", SPELLCATEGORY_FOOD) {}
    };

    class PartyMemberNeedsWaterTrigger : public PartyMemberNeedsSustenanceTrigger
    {
    public:
        PartyMemberNeedsWaterTrigger(PlayerbotAI* ai)
            : PartyMemberNeedsSustenanceTrigger(ai, "party member needs water", SPELLCATEGORY_DRINK) {}
    };
}
