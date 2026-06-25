/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

/**
 * @file Mail.cpp
 * @brief Mail system implementation
 *
 * This file implements the Mail class which manages in-game mail:
 *
 * - Mail creation and sending
 * - Mail with items and money
 * - Mail expiration and deletion
 * - COD (Cash on Delivery) payments
 * - Mail reading and item retrieval
 * - Auction house mail notifications
 *
 * Mails are stored in the `mail` database table and loaded
 * when players log in. System mails can be sent for various
 * game events (auctions, GM messages, etc.).
 *
 * @see Mail for the mail class
 * @see MailHandler for network opcode handling
 */

#include "Mail.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "UpdateMask.h"
#include "Unit.h"
#include "Language.h"
#include "DBCStores.h"
#include "BattleGround/BattleGroundMgr.h"
#include "Item.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/CustodyDeferred.h"

/**
 * Creates a new MailSender object.
 *
 * @param sender The object/player sending this mail.
 * @param stationery The stationary associated with this sender.
 */
MailSender::MailSender(Object* sender, MailStationery stationery) : m_stationery(stationery)
{
    switch (sender->GetTypeId())
    {
        case TYPEID_UNIT:
            m_messageType = MAIL_CREATURE;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_GAMEOBJECT:
            m_messageType = MAIL_GAMEOBJECT;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
            m_messageType = MAIL_ITEM;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_PLAYER:
            m_messageType = MAIL_NORMAL;
            m_senderId = sender->GetGUIDLow();
            if (static_cast<Player *>(sender)->isGameMaster())
            {
                m_stationery = MAIL_STATIONERY_GM;
            }
            break;
        default:
            m_messageType = MAIL_NORMAL;
            m_senderId = 0;                                 // will show mail from nonexistent player
            sLog.outError("MailSender::MailSender - Mail have unexpected sender typeid (%u)", sender->GetTypeId());
            break;
    }
}

/**
 * Creates a new MailSender object from an AuctionEntry.
 *
 * @param sender the AuctionEntry from which this mail is generated.
 */
MailSender::MailSender(AuctionEntry* sender)
    : m_messageType(MAIL_AUCTION), m_senderId(sender->GetHouseId()), m_stationery(MAIL_STATIONERY_AUCTION)
{
}

/**
 * Creates a new MailReceiver object.
 *
 * @param receiver The player receiving the mail.
 */
MailReceiver::MailReceiver(Player* receiver) : m_receiver(receiver), m_receiver_guid(receiver->GetObjectGuid())
{
}

/**
 * Creates a new MailReceiver object with a specified GUID.
 *
 * @param receiver The player receiving the mail.
 * @param receiver_lowguid The GUID to use instead of the receivers.
 */
MailReceiver::MailReceiver(Player* receiver, ObjectGuid receiver_guid) : m_receiver(receiver), m_receiver_guid(receiver_guid)
{
    MANGOS_ASSERT(!receiver || receiver->GetObjectGuid() == receiver_guid);
}

/**
 * Adds an item to the MailDraft.
 *
 * @param item The item to be added to the MailDraft.
 * @returns the MailDraft the item was added to.
 */
MailDraft& MailDraft::AddItem(Item* item)
{
    m_items[item->GetGUIDLow()] = item;
    return *this;
}

/**
 * Prepares the items in a MailDraft.
 */
bool MailDraft::prepareItems(Player* receiver)
{
    if (!m_mailTemplateId || !m_mailTemplateItemsNeed)
    {
        return false;
    }

    m_mailTemplateItemsNeed = false;

    Loot mailLoot(NULL);

    // can be empty
    mailLoot.FillLoot(m_mailTemplateId, LootTemplates_Mail, receiver, true, true);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; m_items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();                           // save for prevent lost at next mail load, if send fail then item will deleted
                AddItem(item);
            }
        }
    }

    return true;
}

/**
 * Deletes the items included in a MailDraft.
 *
 * @param inDB A boolean specifying whether the change should be saved to the database or not.
 */
void MailDraft::deleteIncludedItems(bool inDB /**= false*/)
{
    for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (inDB)
        {
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", item->GetGUIDLow());
        }

        delete item;
    }

    m_items.clear();
}

/**
 * Clone MailDraft from another MailDraft.
 *
 * @param draft Point to source for draft cloning.
 */
void MailDraft::CloneFrom(MailDraft const& draft)
{
    m_mailTemplateId = draft.GetMailTemplateId();
    m_mailTemplateItemsNeed = draft.m_mailTemplateItemsNeed;

    m_subject = draft.GetSubject();
    m_body = draft.GetBody();
    m_money = draft.GetMoney();
    m_COD = draft.GetCOD();

    for (MailItemMap::const_iterator mailItemIter = draft.m_items.begin(); mailItemIter != draft.m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (Item* newitem = item->CloneItem(item->GetCount()))
        {
            newitem->SaveToDB();
            AddItem(newitem);
        }
    }
}

/**
 * Returns a mail to its sender.
 * @param sender_acc           The id of the account of the sender.
 * @param sender_guid          The low part of the GUID of the sender.
 * @param receiver_guid        The low part of the GUID of the receiver.
 */
void MailDraft::SendReturnToSender(uint32 sender_acc, ObjectGuid sender_guid, ObjectGuid receiver_guid)
{
    Player* receiver = sObjectMgr.GetPlayer(receiver_guid);

    uint32 rc_account = 0;
    if (!receiver)
    {
        rc_account = sObjectMgr.GetPlayerAccountIdByGUID(receiver_guid);
    }

    if (!receiver && !rc_account)                           // sender not exist
    {
        deleteIncludedItems(true);
        return;
    }

    // prepare mail and send in other case
    bool needItemDelay = false;

    if (!m_items.empty())
    {
        // if item send to character at another account, then apply item delivery delay
        needItemDelay = sender_acc != rc_account;

        // set owner to new receiver (to prevent delete item with sender char deleting)
        CharacterDatabase.BeginTransaction();
        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            item->SaveToDB();                               // item not in inventory and can be save standalone
            // owner in data will set at mail receive and item extracting
            CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", receiver_guid.GetCounter(), item->GetGUIDLow());
        }
        CharacterDatabase.CommitTransaction();
    }

    // If theres is an item, there is a one hour delivery delay.
    uint32 deliver_delay = needItemDelay ? sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    // will delete item or place to receiver mail list
    SendMailTo(MailReceiver(receiver, receiver_guid), MailSender(MAIL_NORMAL, sender_guid.GetCounter()), MAIL_CHECK_MASK_RETURNED, deliver_delay);
}

/**
 * Sends a mail.
 *
 * @param receiver             The MailReceiver to which this mail is sent.
 * @param sender               The MailSender from which this mail is originated.
 * @param checked              The mask used to specify the mail.
 * @param deliver_delay        The delay after which the mail is delivered in seconds
 */
void MailDraft::SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked, uint32 deliver_delay)
{
    Player* pReceiver = receiver.GetPlayer();               // can be NULL

    uint32 pReceiverAccount = 0;
    if (!pReceiver)
    {
        pReceiverAccount = sObjectMgr.GetPlayerAccountIdByGUID(receiver.GetPlayerGuid());
    }

    if (!pReceiver && !pReceiverAccount)                    // receiver not exist
    {
        deleteIncludedItems(true);
        return;
    }

    bool has_items = !m_items.empty();

    // generate mail template items for online player, for offline player items will generated at open
    if (pReceiver)
    {
        if (prepareItems(pReceiver))
        {
            has_items = true;
        }
    }

    uint32 mailId = sObjectMgr.GenerateMailID();

    time_t deliver_time = time(NULL) + deliver_delay;

    // expire time if COD 3 days, if no COD 30 days, if auction sale pending 1 hour
    uint32 expire_delay;

    // Normal Mail Expire Timer
    expire_delay = 30 * DAY;

    // auction mail without any items and money (auction sale note) pending 1 hour
    if (sender.GetMailMessageType() == MAIL_AUCTION && m_items.empty() && !m_money)
    {
        expire_delay = HOUR;
    }
    // mail from battlemaster (rewardmarks) should last only one day
    else if (sender.GetMailMessageType() == MAIL_CREATURE && sBattleGroundMgr.GetBattleMasterBG(sender.GetSenderId()) != BATTLEGROUND_TYPE_NONE)
    {
        expire_delay = DAY;
    }
    else if (m_COD)
    {
        // COD Mail Expire Timer
        expire_delay = 3 * DAY;
    }
    //Mail from GM
    else if (sender.GetStationery() == MAIL_STATIONERY_GM)
    {
        expire_delay = 90 * DAY;
    }

    time_t expire_time = deliver_time + expire_delay;

    // Add to DB
    CharacterDatabase.BeginTransaction();
    writeMailRows(mailId, receiver, sender, checked, deliver_time, expire_time, has_items);
    CharacterDatabase.CommitTransaction();

    // For online receiver update in game mail status and data
    if (pReceiver)
    {
        pReceiver->AddNewMailDeliverTime(deliver_time);

        Mail* m = new Mail;
        m->messageID = mailId;
        m->mailTemplateId = GetMailTemplateId();
        m->subject = GetSubject();
        m->body = GetBody();
        m->money = GetMoney();
        m->COD = GetCOD();

        for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            m->AddItem(item->GetGUIDLow(), item->GetEntry());
        }

        m->messageType = sender.GetMailMessageType();
        m->stationery = sender.GetStationery();
        m->sender = sender.GetSenderId();
        m->receiverGuid = receiver.GetPlayerGuid();
        m->expire_time = expire_time;
        m->deliver_time = deliver_time;
        m->checked = checked;
        m->state = MAIL_STATE_UNCHANGED;

        pReceiver->AddMail(m);                           // to insert new mail to beginning of maillist

        if (!m_items.empty())
        {
            for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
            {
                pReceiver->AddMItem(mailItemIter->second);
            }
        }
    }
    else if (!m_items.empty())
    {
        deleteIncludedItems();
    }
}

/**
 * Emits the two mail rows (`mail` then `mail_items`) into the CURRENT
 * CharacterDatabase transaction. Shared by SendMailTo (between its own
 * Begin/Commit) and SendMailToInTransaction (appended to the caller's open
 * transaction) so both senders write byte-identical rows.
 *
 * @param mailId               The generated mail id.
 * @param receiver             The MailReceiver to which this mail is sent.
 * @param sender               The MailSender from which this mail is originated.
 * @param checked              The check mask of the mail.
 * @param deliver_time         The computed delivery time.
 * @param expire_time          The computed expiry time.
 * @param has_items            Whether the mail carries items.
 */
void MailDraft::writeMailRows(uint32 mailId, MailReceiver const& receiver, MailSender const& sender,
                              MailCheckMask checked, time_t deliver_time, time_t expire_time,
                              bool has_items)
{
    std::string safe_subject = GetSubject();
    CharacterDatabase.escape_string(safe_subject);

    std::string safe_body = GetBody();
    CharacterDatabase.escape_string(safe_body);

    CharacterDatabase.PExecute("INSERT INTO `mail` (`id`,`messageType`,`stationery`,`mailTemplateId`,`sender`,`receiver`,`subject`,`body`,`has_items`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`) "
        "VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '%u', '" UI64FMTD "','" UI64FMTD "', '%u', '%u', '%u')",
        mailId, sender.GetMailMessageType(), sender.GetStationery(), GetMailTemplateId(), sender.GetSenderId(), receiver.GetPlayerGuid().GetCounter(), safe_subject.c_str(), safe_body.c_str(), (has_items ? 1 : 0), (uint64)expire_time, (uint64)deliver_time, m_money, m_COD, checked);

    for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;
        CharacterDatabase.PExecute("INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('%u', '%u', '%u','%u')",
            mailId, item->GetGUIDLow(), item->GetEntry(), receiver.GetPlayerGuid().GetCounter());
    }
}

/**
 * Custody co-commit variant of SendMailTo.
 *
 * Mirrors SendMailTo's metadata/expiry computation and row writes EXACTLY, but
 * does NOT open or close a transaction: its INSERTs append to the caller's
 * already-open CharacterDatabase transaction so the mail co-commits atomically
 * with the caller's other writes. Every side effect that mutates live world
 * state or destroys a live Item* is queued into @p deferred instead of run
 * inline, so it only happens after the caller's checked commit succeeds.
 *
 * Item lifetime invariant (spec I5): the deferred effects run in the SAME call
 * stack right after the checked commit (no yield/tick in between), so the
 * captured Player and Item pointers are still valid when the closure executes.
 *
 * @param receiver             The MailReceiver to which this mail is sent.
 * @param sender               The MailSender from which this mail is originated.
 * @param deferred             Ordered queue for the deferred online push / item destruction.
 * @param checked              The mask used to specify the mail.
 * @param deliver_delay        The delay after which the mail is delivered in seconds.
 */
void MailDraft::SendMailToInTransaction(MailReceiver const& receiver, MailSender const& sender,
                                        CustodyDeferred& deferred, MailCheckMask checked,
                                        uint32 deliver_delay)
{
    Player* pReceiver = receiver.GetPlayer();               // can be NULL

    uint32 pReceiverAccount = 0;
    if (!pReceiver)
    {
        pReceiverAccount = sObjectMgr.GetPlayerAccountIdByGUID(receiver.GetPlayerGuid());
    }

    if (!pReceiver && !pReceiverAccount)                    // receiver not exist
    {
        // Delete the item rows in the caller's transaction, but DEFER the live
        // Item* destruction until the commit result is known (do not delete
        // inside the open transaction).
        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", item->GetGUIDLow());
            deferred.itemsToDestroyOnRollback.push_back(item);
        }
        m_items.clear();
        return;
    }

    bool has_items = !m_items.empty();

    // generate mail template items for online player, for offline player items will generated at open
    if (pReceiver)
    {
        if (prepareItems(pReceiver))
        {
            has_items = true;
        }
    }

    uint32 mailId = sObjectMgr.GenerateMailID();

    time_t deliver_time = time(NULL) + deliver_delay;

    // expire time if COD 3 days, if no COD 30 days, if auction sale pending 1 hour
    uint32 expire_delay;

    // Normal Mail Expire Timer
    expire_delay = 30 * DAY;

    // auction mail without any items and money (auction sale note) pending 1 hour
    if (sender.GetMailMessageType() == MAIL_AUCTION && m_items.empty() && !m_money)
    {
        expire_delay = HOUR;
    }
    // mail from battlemaster (rewardmarks) should last only one day
    else if (sender.GetMailMessageType() == MAIL_CREATURE && sBattleGroundMgr.GetBattleMasterBG(sender.GetSenderId()) != BATTLEGROUND_TYPE_NONE)
    {
        expire_delay = DAY;
    }
    else if (m_COD)
    {
        // COD Mail Expire Timer
        expire_delay = 3 * DAY;
    }
    //Mail from GM
    else if (sender.GetStationery() == MAIL_STATIONERY_GM)
    {
        expire_delay = 90 * DAY;
    }

    time_t expire_time = deliver_time + expire_delay;

    // Append the mail rows to the caller's OPEN transaction (no Begin/Commit).
    writeMailRows(mailId, receiver, sender, checked, deliver_time, expire_time, has_items);

    // For online receiver, DEFER the in-game mail status/data push until the
    // caller's checked commit has succeeded. Capture everything by value so the
    // closure never aliases this (possibly destroyed) MailDraft.
    if (pReceiver)
    {
        DeferredMailPush push;
        push.mailId = mailId;
        push.receiverGuid = receiver.GetPlayerGuid().GetCounter();
        push.senderId = sender.GetSenderId();
        push.messageType = sender.GetMailMessageType();
        push.stationery = sender.GetStationery();
        push.mailTemplateId = GetMailTemplateId();
        push.subject = GetSubject();
        push.body = GetBody();
        push.money = GetMoney();
        push.cod = GetCOD();
        push.checked = checked;
        push.deliverTime = (uint64)deliver_time;
        push.expireTime = (uint64)expire_time;

        for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            push.items.push_back(std::make_pair(item->GetGUIDLow(), item->GetEntry()));
            push.liveItems.push_back(item);
        }

        // Ownership of the live Item* passes to the deferred push; this draft no
        // longer destroys them on its destructor path.
        m_items.clear();

        // Replays SendMailTo's online push (Mail.cpp online branch) identically.
        deferred.effects.push_back([pReceiver, push]()
        {
            pReceiver->AddNewMailDeliverTime((time_t)push.deliverTime);

            Mail* m = new Mail;
            m->messageID = push.mailId;
            m->mailTemplateId = push.mailTemplateId;
            m->subject = push.subject;
            m->body = push.body;
            m->money = push.money;
            m->COD = push.cod;

            for (std::vector<std::pair<uint32, uint32>>::const_iterator it = push.items.begin();
                 it != push.items.end(); ++it)
            {
                m->AddItem(it->first, it->second);
            }

            m->messageType = push.messageType;
            m->stationery = push.stationery;
            m->sender = push.senderId;
            m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, push.receiverGuid);
            m->expire_time = (time_t)push.expireTime;
            m->deliver_time = (time_t)push.deliverTime;
            m->checked = push.checked;
            m->state = MAIL_STATE_UNCHANGED;

            pReceiver->AddMail(m);                           // to insert new mail to beginning of maillist

            for (std::vector<Item*>::const_iterator it = push.liveItems.begin();
                 it != push.liveItems.end(); ++it)
            {
                pReceiver->AddMItem(*it);
            }
        });
    }
    else if (!m_items.empty())
    {
        // Offline-with-items: the rows are already written to the caller's
        // transaction; DEFER the live Item* destruction until the result is
        // known instead of deleting inside the open transaction.
        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            deferred.itemsToDestroyOnRollback.push_back(mailItemIter->second);
        }
        m_items.clear();
    }
}

/**
 * Destroys the live Item* objects that were to be mailed but are now orphaned
 * because the caller's transaction rolled back.
 */
void CustodyDeferred::discardItems()
{
    for (std::vector<Item*>::iterator it = itemsToDestroyOnRollback.begin();
         it != itemsToDestroyOnRollback.end(); ++it)
    {
        delete *it;
    }
    itemsToDestroyOnRollback.clear();
}

/**
 * Generate items from template at mails loading (this happens when mail with mail template items send in time when receiver has been offline)
 *
 * @param receiver             reciver of mail
 */

void Mail::prepareTemplateItems(Player* receiver)
{
    if (!mailTemplateId || !items.empty())
    {
        return;
    }

    has_items = true;

    Loot mailLoot(NULL);

    // can be empty
    mailLoot.FillLoot(mailTemplateId, LootTemplates_Mail, receiver, true, true);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE `mail` SET `has_items` = 1 WHERE `id` = %u", messageID);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();

                AddItem(item->GetGUIDLow(), item->GetEntry());

                receiver->AddMItem(item);

                CharacterDatabase.PExecute("INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('%u', '%u', '%u','%u')",
                    messageID, item->GetGUIDLow(), item->GetEntry(), receiver->GetGUIDLow());
            }
        }
    }

    CharacterDatabase.CommitTransaction();
}

/*! @} */
