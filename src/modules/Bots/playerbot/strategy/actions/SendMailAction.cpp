#include "botpch.h"
#include "Mail.h"
#include "../../playerbot.h"
#include "SendMailAction.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool SendMailAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(bot->GetObjectGuid());
    if (sPlayerbotAIConfig.IsInRandomAccountList(account))
    {
        ai->TellMaster("I can't do that");
        return false;
    }

    list<ObjectGuid> gos = *context->GetValue<list<ObjectGuid> >("nearest game objects");
    bool mailboxFound = false;
    for (list<ObjectGuid>::iterator i = gos.begin(); i != gos.end(); ++i)
    {
        GameObject* go = ai->GetGameObject(*i);
        if (go && go->GetGoType() == GAMEOBJECT_TYPE_MAILBOX)
        {
            mailboxFound = true;
            break;
        }
    }

    if (!mailboxFound)
    {
        ai->TellMaster("There is no mailbox nearby");
        return false;
    }

    string text = event.getParam();
    Player* receiver = master;
    vector<string> ss = split(text, ' ');
    if (ss.size() > 1)
    {
        Player* p = sObjectMgr.GetPlayer(ss[ss.size() - 1].c_str());
        if (p) receiver = p;
    }

    ItemIds ids = chat->parseItems(text);
    if (ids.size() > 1)
    {
        ai->TellMaster("You can not request more than one item");
        return false;
    }

    if (ids.empty())
    {
        uint32 money = chat->parseMoney(text);
        if (!money)
        {
            return false;
        }

        if (bot->GetMoney() < money)
        {
            ai->TellMaster("I don't have enough money");
            return false;
        }

        ostringstream body;
        body << "Hello, " << receiver->GetName() << ",\n";
        body << "\n";
        body << "Here is the money you asked for";
        body << "\n";
        body << "Thanks,\n";
        body << bot->GetName() << "\n";


        MailDraft draft("Money you asked for", body.str());
        draft.SetMoney(money);
        bot->SetMoney(bot->GetMoney() - money);
        draft.SendMailTo(MailReceiver(receiver), MailSender(bot));

        ostringstream out; out << "Sending mail to " << receiver->GetName();
        ai->TellMaster(out.str());
        return true;
    }

    ostringstream body;
    body << "Hello, " << receiver->GetName() << ",\n";
    body << "\n";
    body << "Here are the item(s) you asked for";
    body << "\n";
    body << "Thanks,\n";
    body << bot->GetName() << "\n";

    MailDraft draft("Item(s) you asked for", body.str());
    for (ItemIds::iterator i =ids.begin(); i != ids.end(); i++)
    {
        FindItemByIdVisitor visitor(*i);
        IterateItems(&visitor, ITERATE_ITEMS_IN_BAGS);
        list<Item*> items = visitor.GetResult();
        for (list<Item*>::iterator i = items.begin(); i != items.end(); ++i)
        {
            Item* item = *i;
            if (item->IsSoulBound() || item->IsConjuredConsumable())
            {
                ostringstream out;
                out << "Cannot send " << ChatHelper::formatItem(item->GetProto());
                ai->TellMaster(out);
                continue;
            }

            ItemPrototype const *proto = item->GetProto();
            bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            item->DeleteFromInventoryDB();
            item->SetOwnerGuid(master->GetObjectGuid());
            item->SaveToDB();
            draft.AddItem(item);
            draft.SendMailTo(MailReceiver(receiver), MailSender(bot));

            ostringstream out; out << "Sending mail to " << receiver->GetName();
            ai->TellMaster(out.str());
            return true;
        }
    }

    return false;
}
