#include "botpch.h"
#include "../../playerbot.h"
#include "CheckMountStateAction.h"

using namespace ai;

uint64 extractGuid(WorldPacket& packet);

bool CheckMountStateAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!bot->GetGroup() || !master)
    {
        return false;
    }
    if (bot->IsTaxiFlying())
    {
        return false;
    }
    if (master->IsTaxiFlying())
    {
        return false;  // not the kind of mounting this is supposed to react to
    }

    if (master->IsMounted() && !bot->IsMounted())
    {
        return Mount();
    }
    else if (!master->IsMounted() && bot->IsMounted())
    {
        WorldPacket emptyPacket;
        bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
        return true;
    }
    return false;
}

class FindMountItemVisitor : public IterateItemsVisitor
{
    public:
        explicit FindMountItemVisitor(int32 minSpeed) : IterateItemsVisitor(), minSpeed(minSpeed), best(nullptr), bestSpeed(-1) {}

        virtual bool Visit(Item* item)
        {
            for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
            {
                uint32 spellId = item->GetProto()->Spells[s].SpellId;
                if (!spellId)
                {
                    continue;
                }

                SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
                if (!spellInfo || spellInfo->EffectAura[0] != SPELL_AURA_MOUNTED)
                {
                    continue;
                }

                int32 speed = max(spellInfo->EffectBasePoints[1], spellInfo->EffectBasePoints[2]);
                if (speed >= minSpeed && speed > bestSpeed)
                {
                    bestSpeed = speed;
                    best = item;
                }
            }
            return true;
        }

        Item* GetResult() { return best; }

    private:
        int32 minSpeed;
        Item* best;
        int32 bestSpeed;
};

bool CheckMountStateAction::Mount()
{
    Player* master = GetMaster();

    if (bot->IsNonMeleeSpellCasted(true))
    {
        return false;
    }

    ai->RemoveShapeshift();
    Unit::AuraList const& auras = master->GetAurasByType(SPELL_AURA_MOUNTED);
    if (auras.empty())
    {
        return false;
    }

    const SpellEntry* masterSpell = master->GetAurasByType(SPELL_AURA_MOUNTED).front()->GetSpellProto();
    int32 masterSpeed = max(masterSpell->EffectBasePoints[1], masterSpell->EffectBasePoints[2]);

    map<int32, vector<uint32> > spells;
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
        {
            continue;
        }

        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo || spellInfo->EffectAura[0] != SPELL_AURA_MOUNTED)
        {
            continue;
        }

        int32 effect = max(spellInfo->EffectBasePoints[1], spellInfo->EffectBasePoints[2]);
        if (effect < masterSpeed)
        {
            continue;
        }

        spells[effect].push_back(spellId);
    }

    for (map<int32, vector<uint32> >::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        vector<uint32>& ids = i->second;
        int index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        ai->CastSpell(ids[index], bot);
        ai->SetNextCheckDelay(4000);
        return true;
    }

    FindMountItemVisitor visitor(masterSpeed);
    IterateItems(&visitor);
    Item* mountItem = visitor.GetResult();
    if (!mountItem)
    {
        return false;
    }

    SpellCastTargets targets;
    targets.m_targetMask = TARGET_FLAG_SELF;
    targets.setUnitTarget(bot);
    bot->CastItemUseSpell(mountItem, targets);
    ai->SetNextCheckDelay(4000);
    return true;
}
