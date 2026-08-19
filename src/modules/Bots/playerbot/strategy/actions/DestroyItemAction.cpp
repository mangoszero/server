#include "botpch.h"
#include "../../playerbot.h"
#include "DestroyItemAction.h"

#include "../values/ItemCountValue.h"

using namespace ai;

bool DestroyItemAction::Execute(Event event)
{
    string text = event.getParam();
    ItemIds ids = chat->parseItems(text);

    for (ItemIds::iterator i =ids.begin(); i != ids.end(); i++)
    {
        FindItemByIdVisitor visitor(*i);
        DestroyItem(&visitor);
    }

    return true;
}

void DestroyItemAction::DestroyItem(FindItemVisitor* visitor)
{
    IterateItems(visitor);
    list<Item*> items = visitor->GetResult();
    for (list<Item*>::iterator i = items.begin(); i != items.end(); ++i)
    {
        Item* item = *i;

        // The message is built BEFORE the item is destroyed. Player::DestroyItem deletes the
        // Item, so reading its prototype afterwards was a use-after-free -- one that survived
        // in practice only because the freed block was usually still mapped and not yet
        // handed to another allocation.
        ostringstream out; out << chat->formatItem(item->GetProto()) << " destroyed";

        bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        bot->SaveInventoryAndGoldToDB();
        ai->TellMaster(out);
    }
}
