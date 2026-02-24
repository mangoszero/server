#include "botpch.h"
#include "../../playerbot.h"
#include "MageActions.h"

using namespace ai;

Value<Unit*>* CastPolymorphAction::GetTargetValue()
{
    return context->GetValue<Unit*>("cc target", getName());
}

static Player* FindPartyMemberWithoutSustenance(Player* bot, bool food)
{
    Group* group = bot->GetGroup();
    if (!group)
        return NULL;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->getSource();
        if (!player || player == bot || !player->IsAlive())
        {
            continue;
        }
        if (player->GetMapId() != bot->GetMapId())
        {
            continue;
        }
        if (!bot->IsWithinDist(player, sPlayerbotAIConfig.sightDistance))
        {
            continue;
        }
        if (!food && player->GetPowerType() != POWER_MANA)
        {
            continue;
        }
        bool hasItem;
        if (food)
        {
            FindConjuredFoodVisitor foodVisitor(player, SPELLCATEGORY_FOOD);
            hasItem = InventoryAction::FindPlayerItem(player, &foodVisitor) != NULL;
        }
        else
        {
            FindConjuredFoodVisitor drinkVisitor(player, SPELLCATEGORY_DRINK);
            hasItem = InventoryAction::FindPlayerItem(player, &drinkVisitor) != NULL;
        }
        if (!hasItem)
            return player;
    }

    return NULL;
}

bool GiveConjuredFoodAction::isUseful()
{
    if (bot->IsInCombat())
        return false;
    if (!bot->GetGroup())
        return false;

    return !AI_VALUE2(list<Item*>, "inventory items", "conjured food").empty();
}

bool GiveConjuredFoodAction::Execute(Event event)
{
    list<Item*> foods = AI_VALUE2(list<Item*>, "inventory items", "conjured food");
    if (foods.empty())
    {
        return false;
    }
    Item* food = foods.front();

    Player* target = FindPartyMemberWithoutSustenance(bot, true);
    if (!target)
    {
        return false;
    }

    uint32 itemId = food->GetEntry();
    uint32 count = food->GetCount();

    Item* newItem = target->StoreNewItemInInventorySlot(itemId, count);
    if (!newItem)
        return false;

    bot->DestroyItem(food->GetBagSlot(), food->GetSlot(), true);
    newItem->AddToUpdateQueueOf(target);

    Group* group = bot->GetGroup();
    if (group)
    {
        ostringstream out;
        out << "Have some food, " << target->GetName();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_PARTY, out.str().c_str(),
            LANG_UNIVERSAL, bot->GetChatTag(), bot->GetObjectGuid(), bot->GetName());
        group->BroadcastPacket(&data, false);
    }
    return true;
}

bool GiveConjuredWaterAction::isUseful()
{
    if (bot->IsInCombat())
        return false;
    if (!bot->GetGroup())
        return false;

    return !AI_VALUE2(list<Item*>, "inventory items", "conjured drink").empty();
}

bool GiveConjuredWaterAction::Execute(Event event)
{
    list<Item*> drinks = AI_VALUE2(list<Item*>, "inventory items", "conjured drink");
    if (drinks.empty())
        return false;
    Item* water = drinks.front();

    Player* target = FindPartyMemberWithoutSustenance(bot, false);
    if (!target)
        return false;

    uint32 itemId = water->GetEntry();
    uint32 count = water->GetCount();

    Item* newItem = target->StoreNewItemInInventorySlot(itemId, count);
    if (!newItem)
        return false;

    bot->DestroyItem(water->GetBagSlot(), water->GetSlot(), true);
    newItem->AddToUpdateQueueOf(target);

    Group* group = bot->GetGroup();
    if (group)
    {
        ostringstream out;
        out << "Have some water, " << target->GetName();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_PARTY, out.str().c_str(),
            LANG_UNIVERSAL, bot->GetChatTag(), bot->GetObjectGuid(), bot->GetName());
        group->BroadcastPacket(&data, false);
    }
    return true;
}

uint32 CastConjureManaGemAction::FindBestConjureManaSpell()
{
    uint32 bestSpellId = 0;

    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
            continue;

        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (!pSpellInfo)
            continue;

        for (int i = 0; i < MAX_EFFECT_INDEX; i++)
        {
            if (pSpellInfo->Effect[i] != SPELL_EFFECT_CREATE_ITEM)
                continue;

            uint32 itemId = pSpellInfo->EffectItemType[i];
            if (!itemId)
                continue;

            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto ||
                !(proto->Flags & ITEM_FLAG_CONJURED) ||
                proto->Class != ITEM_CLASS_CONSUMABLE ||
                proto->SubClass == ITEM_SUBCLASS_POTION)
                continue;

            bool isManaGem = false;
            for (int j = 0; j < MAX_ITEM_PROTO_SPELLS && !isManaGem; j++)
            {
                const SpellEntry* itemSpell = sSpellStore.LookupEntry(proto->Spells[j].SpellId);
                if (!itemSpell)
                    continue;
                for (int k = 0; k < MAX_EFFECT_INDEX; k++)
                {
                    if (itemSpell->Effect[k] == SPELL_EFFECT_ENERGIZE)
                    {
                        isManaGem = true;
                        break;
                    }
                }
            }

            if (isManaGem && spellId > bestSpellId)
                bestSpellId = spellId;

            break;
        }
    }

    return bestSpellId;
}

bool CastConjureManaGemAction::isPossible()
{
    m_bestSpellId = FindBestConjureManaSpell();
    return m_bestSpellId != 0;
}

bool CastConjureManaGemAction::Execute(Event event)
{
    if (!m_bestSpellId)
        return false;

    return ai->CastSpell(m_bestSpellId, GetTarget());
}

