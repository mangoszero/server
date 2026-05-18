#pragma once

#include "../Action.h"

namespace ai
{
    class UseItemAction : public Action {
    public:
        UseItemAction(PlayerbotAI* ai, string name = "use", bool selfOnly = false) : Action(ai, name), selfOnly(selfOnly) {}

    public:
        virtual bool Execute(Event event);
        virtual bool isPossible();

    protected:
        bool UseItemAuto(Item* item);

    private:
        bool UseItemOnGameObject(Item* item, ObjectGuid go);
        bool UseItemOnItem(Item* item, Item* itemTarget);
        bool UseItem(Item* item, ObjectGuid go, Item* itemTarget);
        bool UseGameObject(ObjectGuid guid);
        bool SocketItem(Item* item, Item* gem, bool replace = false);

    private:
        bool selfOnly;
    };

    class UseSpellItemAction : public UseItemAction {
    public:
        UseSpellItemAction(PlayerbotAI* ai, string name, bool selfOnly = false) : UseItemAction(ai, name, selfOnly) {}

    public:
        virtual bool isUseful();
    };

    class UseHealingPotion : public UseItemAction {
    public:
        UseHealingPotion(PlayerbotAI* ai) : UseItemAction(ai, "healing potion") {}
        virtual bool isUseful() { return AI_VALUE2(bool, "combat", "self target"); }
    };

    class UseBandage : public UseItemAction
    {
    public:
        UseBandage(PlayerbotAI* ai) : UseItemAction(ai, "bandage"), m_isBandaging(false) {}

        virtual bool Execute(Event event)
        {
            if (m_isBandaging)
            {
                ai->SetNextCheckDelay(500);
                return true;
            }

            list<Item*> items = AI_VALUE2(list<Item*>, "inventory items", "bandage");
            if (items.empty())
                return false;

            bool result = UseItemAuto(*items.begin());
            if (result)
            {
                m_isBandaging = true;
                ai->SetNextCheckDelay(500);
            }
            return result;
        }

        virtual bool isPossible()
        {
            if (m_isBandaging && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                m_isBandaging = false;
            if (m_isBandaging)
                return true;
            return UseItemAction::isPossible();
        }

        virtual bool isUseful()
        {
            if (bot->HasAura(SPELL_ID_RECENTLY_BANDAGED))
                return false;
            if (AI_VALUE2(bool, "combat", "self target"))
                return bot->getAttackers().empty();
            return bot->GetGroup() &&
                   AI_VALUE2(list<Item*>, "inventory items", "food").empty();
        }

    private:
        bool m_isBandaging;
    };

    class UseManaPotion : public UseItemAction
    {
    public:
        UseManaPotion(PlayerbotAI* ai) : UseItemAction(ai, "mana potion") {}
        virtual bool isUseful() { return AI_VALUE2(bool, "combat", "self target"); }
        virtual bool Execute(Event event)
        {
            list<Item*> gems = AI_VALUE2(list<Item*>, "inventory items", "mana gem");
            if (!gems.empty())
                return UseItemAuto(*gems.begin());
            return UseItemAction::Execute(event);
        }
    };
}
