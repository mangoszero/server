/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Chat.h"
#include "Language.h"
#include "Mail.h"
#include "MassMailMgr.h"

 /**********************************************************************
      CommandTable : mailCommandTable
 /***********************************************************************/
// Send mail by command
bool ChatHandler::HandleSendMailCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // format: name "subject text" "mail text"
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    MailDraft draft;

    // Subject and content should not be empty :
    if (!*args)
    {
        return false;
    }
    else
    {
        // fill draft
        if (!HandleSendMailHelper(draft, args))
        {
            return false;
        }
    }


    // GM mail
    MailSender sender(MAIL_NORMAL, m_session ? m_session->GetPlayer()->GetGUIDLow() : (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(target, target_guid), sender);

    std::string nameLink = playerLink(target_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMailHelper(MailDraft& draft, char* args)
{
    // format: "subject text" "mail text"
    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    // msgSubject, msgText isn't NUL after prev. check
    draft.SetSubjectAndBody(msgSubject, msgText);

    return true;
}

bool ChatHandler::HandleSendMassMailCommand(char* args)
{
    // format: raceMask "subject text" "mail text"
    uint32 raceMask = 0;
    char const* name = NULL;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    // need dynamic object because it trasfered to mass mailer
    MailDraft* draft = new MailDraft;

    // fill mail
    if (!HandleSendMailHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    // GM mail
    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendItemsHelper(MailDraft& draft, char* args)
{
    // format: "subject text" "mail text" item1[:count1][:enchant1] item2[:count2][:enchant2] ... item12[:count12][:enchant12]
    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    // extract items
    typedef std::tuple<uint32, uint32, uint32> ItemToSend;
    typedef std::list< ItemToSend > ItemsToSend;
    ItemsToSend items;

    // get from tail next item str
    while (char* itemStr = ExtractArg(&args))
    {
        // parse item str
        uint32 item_id = 0;
        uint32 item_count = 1;
        uint32 item_enchant_id = 0;

        // Try with item1[:count1][:enchant1] format
        if (sscanf(itemStr, "%u:%u:%u", &item_id, &item_count, &item_enchant_id) == 0)
        {
            // Try perhaps with item1[:count1]
            if (sscanf(itemStr, "%u:%u", &item_id, &item_count) == 0)
            {
                // Try at least with item1 - if not a integer >0 then => error
                if (sscanf(itemStr, "%u", &item_id) == 0)
                {
                    return false;
                }
            }
        }

        if (!item_id)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        ItemPrototype const* item_proto = ObjectMgr::GetItemPrototype(item_id);
        if (!item_proto)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        if (item_count < 1 || (item_proto->MaxCount > 0 && item_count > uint32(item_proto->MaxCount)))
        {
            PSendSysMessage(LANG_COMMAND_INVALID_ITEM_COUNT, item_count, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        uint32 max_items_count = item_proto->GetMaxStackSize();
        uint32 remaining_items_count = item_count;

        while (remaining_items_count > max_items_count)
        {
            items.push_back(ItemToSend(item_id, max_items_count, item_enchant_id));
            remaining_items_count -= max_items_count;
        }

        items.push_back(ItemToSend(item_id, remaining_items_count, item_enchant_id));

        if (items.size() > MAX_MAIL_ITEMS)
        {
            PSendSysMessage(LANG_COMMAND_MAIL_ITEMS_LIMIT, MAX_MAIL_ITEMS);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // fill mail
    draft.SetSubjectAndBody(msgSubject, msgText);

    for (ItemsToSend::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        uint32 item_id =  std::get<0>(*itr);
        uint32 item_count = std::get<1>(*itr);
        if (Item* item = Item::CreateItem(item_id, item_count, m_session ? m_session->GetPlayer() : 0))
        {
            uint32 item_enchant_id = std::get<2>(*itr);
            if (item_enchant_id) {
                item->SetEnchantment(PERM_ENCHANTMENT_SLOT, item_enchant_id, 0, 0);
            }
            item->SaveToDB();                               // save for prevent lost at next mail load, if send fail then item will deleted
            draft.AddItem(item);
        }
    }

    return true;
}

bool ChatHandler::HandleSendItemsCommand(char* args)
{
    // format: "subject text" "mail text" item1[:count1][:enchant1] item2[:count2][:enchant2] ... item12[:count12][:enchant12]
    Player* receiver;
    ObjectGuid receiver_guid;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
    {
        return false;
    }

    MailDraft draft;

    // fill mail
    if (!HandleSendItemsHelper(draft, args))
    {
        return false;
    }

    MailSender sender(MAIL_NORMAL, m_session ? m_session->GetPlayer()->GetGUIDLow() : (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassItemsCommand(char* args)
{
    // format: racemask "subject text" "mail text" item1[:count1] item2[:count2] ... item12[:count12]

    uint32 raceMask = 0;
    char const* name = NULL;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    // need dynamic object because it trasfered to mass mailer
    MailDraft* draft = new MailDraft;


    // fill mail
    if (!HandleSendItemsHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendMoneyHelper(MailDraft& draft, char* args)
{
    /// format: "subject text" "mail text" money

    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    uint32 money;
    if (!ExtractUInt32(&args, money))
    {
        return false;
    }

    if (money <= 0)
    {
        return false;
    }

    // msgSubject, msgText isn't NUL after prev. check
    draft.SetSubjectAndBody(msgSubject, msgText).SetMoney(money);

    return true;
}

bool ChatHandler::HandleSendMoneyCommand(char* args)
{
    /// format: name "subject text" "mail text" money

    Player* receiver;
    ObjectGuid receiver_guid;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
    {
        return false;
    }

    MailDraft draft;

    // fill mail
    if (!HandleSendMoneyHelper(draft, args))
    {
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassMoneyCommand(char* args)
{
    /// format: raceMask "subject text" "mail text" money

    uint32 raceMask = 0;
    char const* name = NULL;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    // need dynamic object because it trasfered to mass mailer
    MailDraft* draft = new MailDraft;

    // fill mail
    if (!HandleSendMoneyHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

