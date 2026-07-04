#include "botpch.h"
#include "../../playerbot.h"
#include "RestoreWeaponsAction.h"
#include "../values/ItemCountValue.h"
#include "InventoryAction.h"

using namespace ai;

class FindItemByGuidVisitor : public FindItemVisitor
{
    public:
        explicit FindItemByGuidVisitor(ObjectGuid guid) : FindItemVisitor(), m_guid(guid) {}

        bool Accept(const ItemPrototype* proto) override
        {
            return false;
        }

        bool Visit(Item* item) override
        {
            if (item->GetObjectGuid() == m_guid)
            {
                m_found = item;
                return false;
            }
            return true;
        }

        Item* GetFound() const
        {
            return m_found;
        }

    private:
        ObjectGuid m_guid;
        Item* m_found = nullptr;
};

bool RestoreWeaponsAction::isUseful()
{
    return AI_VALUE(ObjectGuid, "saved mainhand weapon") != ObjectGuid();
}

bool RestoreWeaponsAction::Execute(Event event)
{
    bot->InterruptNonMeleeSpells(false);

    ObjectGuid mainhandGuid = AI_VALUE(ObjectGuid, "saved mainhand weapon");
    ObjectGuid offhandGuid = AI_VALUE(ObjectGuid, "saved offhand weapon");

    bool restoredMainhand = false;

    if (mainhandGuid != ObjectGuid())
    {
        FindItemByGuidVisitor visitor(mainhandGuid);
        InventoryAction::FindPlayerItem(bot, &visitor);
        if (Item* pItem = visitor.GetFound())
        {
            restoredMainhand = EquipFromBag(pItem);
        }
    }

    if (offhandGuid != ObjectGuid())
    {
        FindItemByGuidVisitor visitor(offhandGuid);
        InventoryAction::FindPlayerItem(bot, &visitor);
        if (Item* pItem = visitor.GetFound())
        {
            EquipFromBag(pItem);
        }
    }

    context->GetValue<ObjectGuid>("saved mainhand weapon")->Set(ObjectGuid());
    context->GetValue<ObjectGuid>("saved offhand weapon")->Set(ObjectGuid());

    return restoredMainhand;
}

bool RestoreWeaponsAction::EquipFromBag(Item* pItem)
{
    return InventoryAction::EquipFromBag(bot, pItem);
}
