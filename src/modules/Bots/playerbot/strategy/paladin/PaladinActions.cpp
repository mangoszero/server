#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinActions.h"
#include "PaladinTriggers.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool CastBlessingOnPartyAction::Execute(Event event)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    bool casted = false;

    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = sObjectMgr.GetPlayer(itr->guid);
        if (!member || !member->IsAlive())
        {
            continue;
        }

        if (HasAnyBlessing(ai, member))
        {
            continue;
        }

        string primary;
        switch (member->getClass())
        {
            case CLASS_WARRIOR:
            case CLASS_ROGUE:
            case CLASS_HUNTER:
                primary = "greater blessing of might";
                break;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
                primary = "greater blessing of wisdom";
                break;
            case CLASS_PALADIN:
                primary = "greater blessing of wisdom";
                break;
            default:
                primary = "blessing of might";
                break;
        }

        if (ai->CastSpell(primary, member))
        {
            casted = true;
        }
        else
        {
            string lesser = (primary.find("greater ") == 0) ? primary.substr(8) : primary;
            if (ai->CastSpell(lesser, member))
            {
                casted = true;
            }
            else if (lesser != "blessing of might" && ai->CastSpell("blessing of might", member))
            {
                casted = true;
            }
        }
    }
    return casted;
}

bool CastBlessingOnPartyAction::isUseful()
{
    return bot->GetGroup() != nullptr;
}

