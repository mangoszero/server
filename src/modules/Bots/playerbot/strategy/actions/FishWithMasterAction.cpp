#include "botpch.h"
#include "../../playerbot.h"
#include "FishWithMasterAction.h"
#include "../values/ItemCountValue.h"
#include "../ItemVisitors.h"
 #include "InventoryAction.h"

using namespace ai;

FishWithMasterAction::FishWithMasterAction(PlayerbotAI* ai)
    : Action(ai, "fish with master"), m_state(FISH_STATE_IDLE), m_stateStart(0)
{
}

bool FishWithMasterAction::isUseful()
{
    return AI_VALUE(bool, "master is fishing");
}

bool FishWithMasterAction::isPossible()
{
    Player* master = GetMaster();
    if (!master || bot->GetSkillValue(SKILL_FISHING) == 0 || bot->IsMounted())
    {
        return false;
    }

    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (mainHand && mainHand->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    {
        return true;
    }

    return FindFishingPole() != nullptr;
}

class FindFishingPoleVisitor : public FindItemVisitor
{
     bool Accept(const ItemPrototype* proto) override
     {
         return proto->Class == ITEM_CLASS_WEAPON &&
                proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE;
     }
 };

Item* FishWithMasterAction::FindFishingPole()
{
    FindFishingPoleVisitor visitor;
    return InventoryAction::FindPlayerItem(bot, &visitor);
}

void FishWithMasterAction::SaveWeapons()
{
    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (mainHand)
    {
        context->GetValue<ObjectGuid>("saved mainhand weapon")->Set(mainHand->GetObjectGuid());
    }

    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (offHand)
    {
        context->GetValue<ObjectGuid>("saved offhand weapon")->Set(offHand->GetObjectGuid());
    }
}

bool FishWithMasterAction::AutoEquipFromBag(Item* pItem)
{
    return InventoryAction::EquipFromBag(bot, pItem);
}

void FishWithMasterAction::EquipFishingPole()
{
    Item* pole = FindFishingPole();
    if (pole)
    {
        AutoEquipFromBag(pole);
    }
}

bool FishWithMasterAction::MoveToFishingSpot()
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    float distance = AI_VALUE2(float, "distance", "master target");
    if (distance > sPlayerbotAIConfig.followDistance + 0.1f)
    {
        float spread = (bot->GetObjectGuid().GetCounter() % 8) * (M_PI_F / 4) - M_PI_F;
        bot->GetMotionMaster()->MoveFollow(master, sPlayerbotAIConfig.followDistance,
            bot->GetAngle(master) + spread);
        return false;
    }

    GameObject* masterBobber = AI_VALUE(GameObject*, "master bobber");
    if (masterBobber)
    {
        bot->SetFacingTo(bot->GetAngle(masterBobber));
    }

    bot->GetMotionMaster()->MoveIdle();
    return true;
}

bool FishWithMasterAction::CastFishing()
{
    Spell* currentChannel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (currentChannel && currentChannel->m_spellInfo->ID == 7620)
    {
        if (!bot->GetChannelObjectGuid())
        {
            bot->FinishSpell(CURRENT_CHANNELED_SPELL);
        }
        else
        {
            return true;
        }
    }

    bot->clearUnitState(UNIT_STAT_CHASE);
    bot->clearUnitState(UNIT_STAT_FOLLOW);
    bot->GetMotionMaster()->MoveIdle();

    GameObject* masterBobber = AI_VALUE(GameObject*, "master bobber");
    if (masterBobber)
    {
        bot->SetFacingTo(bot->GetAngle(masterBobber));
    }

    bot->CastSpell(bot, 7620, false);
    return true;
}

GameObject* FishWithMasterAction::GetOwnBobber()
{
    ObjectGuid channelGuid = bot->GetChannelObjectGuid();
    if (!channelGuid)
    {
        return nullptr;
    }

    return bot->GetMap()->GetGameObject(channelGuid);
}

bool FishWithMasterAction::CheckBobberAndLoot()
{
    GameObject* bobber = GetOwnBobber();
    if (!bobber)
    {
        return false;
    }

    if (bobber->getLootState() == GO_READY)
    {
        bobber->Use(bot);
        return true;
    }

    return false;
}

void FishWithMasterAction::CleanupFishing()
{
    Spell* currentChannel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (currentChannel && currentChannel->m_spellInfo->ID == 7620)
    {
        bot->FinishSpell(CURRENT_CHANNELED_SPELL);
    }

    m_state = FISH_STATE_IDLE;
}

bool FishWithMasterAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        CleanupFishing();
        return false;
    }

    if (AI_VALUE2(float, "distance", "master target") > sPlayerbotAIConfig.reactDistance)
    {
        CleanupFishing();
        return false;
    }

    if (!isPossible())
    {
        CleanupFishing();
        return false;
    }

    Item* equippedMainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    bool holdingPole = equippedMainHand &&
        equippedMainHand->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE;
    if (!holdingPole && m_state != FISH_STATE_IDLE && m_state != FISH_STATE_EQUIPPING)
    {
        m_state = FISH_STATE_IDLE;
    }

    switch (m_state)
    {
        case FISH_STATE_IDLE:
        {
            Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if (mainHand && mainHand->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
            {
                m_state = FISH_STATE_MOVING;
                m_stateStart = time(nullptr);
                return true;
            }
            SaveWeapons();
            EquipFishingPole();
            m_state = FISH_STATE_EQUIPPING;
            m_stateStart = time(nullptr);
            return true;
        }
        case FISH_STATE_EQUIPPING:
        {
            Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if (mainHand && mainHand->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
            {
                m_state = FISH_STATE_MOVING;
                m_stateStart = time(nullptr);
                return true;
            }
            if (time(nullptr) - m_stateStart > 1)
            {
                m_state = FISH_STATE_IDLE;
                return false;
            }
            return true;
        }
        case FISH_STATE_MOVING:
        {
            if (MoveToFishingSpot())
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
            }
            else if (time(nullptr) - m_stateStart > 2)
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
            }
            return true;
        }
        case FISH_STATE_CASTING:
        {
            CastFishing();
            GameObject* bobber = GetOwnBobber();
            if (bobber)
            {
                m_state = FISH_STATE_WAITING;
                m_stateStart = time(nullptr);
                return true;
            }
            if (time(nullptr) - m_stateStart > 5)
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
            }
            return true;
        }
        case FISH_STATE_WAITING:
        {
            if (CheckBobberAndLoot())
            {
                m_state = FISH_STATE_LOOTING;
                m_stateStart = time(nullptr);
                return true;
            }
            if (!GetOwnBobber())
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
                return true;
            }
            if (time(nullptr) - m_stateStart > 20)
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
            }
            return true;
        }
        case FISH_STATE_LOOTING:
        {
            Spell* currentChannel = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            if (!currentChannel || currentChannel->m_spellInfo->ID != 7620)
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
                return true;
            }
            if (time(nullptr) - m_stateStart > 3)
            {
                m_state = FISH_STATE_CASTING;
                m_stateStart = time(nullptr);
            }
            return true;
        }
    }

    return false;
}
