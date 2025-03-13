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

#include "GossipDef.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Formulas.h"

// Constructor for GossipMenu, initializes the session and reserves space for menu items
GossipMenu::GossipMenu(WorldSession* session) : m_session(session)
{
    m_gItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
    m_gMenuId = 0;
}

// Destructor for GossipMenu, clears the menu items
GossipMenu::~GossipMenu()
{
    ClearMenu();
}

// Adds a menu item to the gossip menu
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded)
{
    MANGOS_ASSERT(m_gItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    GossipMenuItem gItem;

    gItem.m_gIcon       = Icon;
    gItem.m_gMessage    = Message;
    gItem.m_gCoded      = Coded;
    gItem.m_gSender     = dtSender;
    gItem.m_gOptionId   = dtAction;
    gItem.m_gBoxMessage = BoxMessage;
    gItem.m_gBoxMoney   = BoxMoney;
    m_gItems.push_back(gItem);
}

// Adds gossip menu item data
void GossipMenu::AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script)
{
    GossipMenuItemData pItemData{};

    pItemData.m_gAction_menu    = action_menu;
    pItemData.m_gAction_poi     = action_poi;
    pItemData.m_gAction_script  = action_script;

    m_gItemsData.push_back(pItemData);
}

// Overloaded method to add a menu item with fewer parameters
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, bool Coded)
{
    AddMenuItem(Icon, Message, 0, 0, "", Coded);
}

// Overloaded method to add a menu item with a C-style string message
void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), Coded);
}

// Overloaded method to add a menu item with sender and action parameters
void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, "", 0, Coded);
}

// Overloaded method to add a menu item with box message and money parameters
void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, std::string(BoxMessage ? BoxMessage : ""), BoxMoney, Coded);
}

// Overloaded method to add a menu item with item text parameters
void GossipMenu::AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, bool Coded)
{
    uint32 loc_idx = m_session->GetSessionDbLocaleIndex();

    char const* item_text = itemText ? sObjectMgr.GetMangosString(itemText, loc_idx) : "";
    char const* box_text = boxText ? sObjectMgr.GetMangosString(boxText, loc_idx) : "";

    AddMenuItem(Icon, std::string(item_text), dtSender, dtAction, std::string(box_text), Coded);
}

// Returns the sender ID of a menu item
uint32 GossipMenu::MenuItemSender(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gSender;
}

// Returns the action ID of a menu item
uint32 GossipMenu::MenuItemAction(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gOptionId;
}

// Returns whether a menu item is coded
bool GossipMenu::MenuItemCoded(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gCoded;
}

// Clears all menu items
void GossipMenu::ClearMenu()
{
    m_gItems.clear();
    m_gItemsData.clear();
    m_gMenuId = 0;
}

// Constructor for PlayerMenu, initializes the gossip menu
PlayerMenu::PlayerMenu(WorldSession* session) : mGossipMenu(session)
{
}

// Destructor for PlayerMenu, clears all menus
PlayerMenu::~PlayerMenu()
{
    ClearMenus();
}

// Clears all menus in the player menu
void PlayerMenu::ClearMenus()
{
    mGossipMenu.ClearMenu();
    mQuestMenu.ClearMenu();
}

// Returns the sender ID of a gossip option
uint32 PlayerMenu::GossipOptionSender(unsigned int Selection) const
{
    return mGossipMenu.MenuItemSender(Selection);
}

// Returns the action ID of a gossip option
uint32 PlayerMenu::GossipOptionAction(unsigned int Selection) const
{
    return mGossipMenu.MenuItemAction(Selection);
}

// Returns whether a gossip option is coded
bool PlayerMenu::GossipOptionCoded(unsigned int Selection) const
{
    return mGossipMenu.MenuItemCoded(Selection);
}

// Sends the gossip menu to the player
void PlayerMenu::SendGossipMenu(uint32 TitleTextId, ObjectGuid objectGuid)
{
    WorldPacket data(SMSG_GOSSIP_MESSAGE, (100)); // guess size
    data << ObjectGuid(objectGuid);
    data << uint32(TitleTextId);
    data << uint32(mGossipMenu.MenuItemCount());            // [ZERO] max count 15

    for (uint32 iI = 0; iI < mGossipMenu.MenuItemCount(); ++iI)
    {
        GossipMenuItem const& gItem = mGossipMenu.GetItem(iI);
        data << uint32(iI);
        data << uint8(gItem.m_gIcon);
        data << uint8(gItem.m_gCoded);                      // makes pop up box password
        //data << uint32(gItem.m_gBoxMoney);
        data << gItem.m_gMessage;                           // text for gossip item, max 0x800
       // data << gItem.m_gBoxMessage;
    }

    data << uint32(mQuestMenu.MenuItemCount()); // max count 0x20

    for (uint32 iI = 0; iI < mQuestMenu.MenuItemCount(); ++iI)
    {
        QuestMenuItem const& qItem = mQuestMenu.GetItem(iI);
        uint32 questID = qItem.m_qId;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID);

        data << uint32(questID);
        data << uint32(qItem.m_qIcon);
        data << int32(pQuest->GetQuestLevel());

        int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

        data << title; // max 0x200
    }

    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_MESSAGE from %s", objectGuid.GetString().c_str());
}

// Closes the gossip menu
void PlayerMenu::CloseGossip() const
{
    WorldPacket data(SMSG_GOSSIP_COMPLETE, 0);
    GetMenuSession()->SendPacket(&data);

    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_COMPLETE");
}

// Sends a point of interest to the player (outdated method)
void PlayerMenu::SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName) const
{
    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(Flags);
    data << float(X);
    data << float(Y);
    data << uint32(Icon);
    data << uint32(Data);
    data << locName;

    GetMenuSession()->SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

// Sends a point of interest to the player by ID
void PlayerMenu::SendPointOfInterest(uint32 poi_id) const
{
    PointOfInterest const* poi = sObjectMgr.GetPointOfInterest(poi_id);
    if (!poi)
    {
        sLog.outErrorDb("Requested send nonexistent POI (Id: %u), ignore.", poi_id);
        return;
    }

    std::string icon_name = poi->icon_name;

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (PointOfInterestLocale const* pl = sObjectMgr.GetPointOfInterestLocale(poi_id))
        {
            if (pl->IconName.size() > size_t(loc_idx) && !pl->IconName[loc_idx].empty())
            {
                icon_name = pl->IconName[loc_idx];
            }
        }
    }

    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(poi->flags);
    data << float(poi->x);
    data << float(poi->y);
    data << uint32(poi->icon);
    data << uint32(poi->data);
    data << icon_name;

    GetMenuSession()->SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

// Sends a talking message to the player by text ID
void PlayerMenu::SendTalking(uint32 textID) const
{
    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100); // guess size
    data << textID; // can be < 0

    if (!pGossip)
    {
        for (uint32 i = 0; i < 8; ++i)
        {
            data << float(0);
            data << "Greetings $N";
            data << "Greetings $N";
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
        }
    }
    else
    {
        std::string Text_0[MAX_GOSSIP_TEXT_OPTIONS], Text_1[MAX_GOSSIP_TEXT_OPTIONS];
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            Text_0[i] = pGossip->Options[i].Text_0;
            Text_1[i] = pGossip->Options[i].Text_1;
        }

        int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();

        sObjectMgr.GetNpcTextLocaleStringsAll(textID, loc_idx, &Text_0, &Text_1);

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << pGossip->Options[i].Probability;

            if (Text_0[i].empty())
            {
                data << Text_1[i];
            }
            else
            {
                data << Text_0[i];
            }

            if (Text_1[i].empty())
            {
                data << Text_0[i];
            }
            else
            {
                data << Text_1[i];
            }

            data << pGossip->Options[i].Language;

            for (int j = 0; j < 3; ++j)
            {
                data << pGossip->Options[i].Emotes[j]._Delay;
                data << pGossip->Options[i].Emotes[j]._Emote;
            }
        }
    }
    GetMenuSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE ");
}

// Sends a talking message to the player by title and text
void PlayerMenu::SendTalking(char const* title, char const* text) const
{
    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 50); // guess size
    data << uint32(0);
    for (uint32 i = 0; i < 8; ++i)
    {
        data << float(0);
        data << title;
        data << text;
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
    }

    GetMenuSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE ");
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

// Constructor for QuestMenu, reserves space for quest items
QuestMenu::QuestMenu()
{
    m_qItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
}

// Destructor for QuestMenu, clears the menu items
QuestMenu::~QuestMenu()
{
    ClearMenu();
}

// Adds a quest menu item to the quest menu
void QuestMenu::AddMenuItem(uint32 QuestId, uint8 Icon)
{
    Quest const* qinfo = sObjectMgr.GetQuestTemplate(QuestId);
    if (!qinfo)
    {
        return;
    }

    MANGOS_ASSERT(m_qItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    QuestMenuItem qItem{};

    qItem.m_qId   = QuestId;
    qItem.m_qIcon = Icon;

    m_qItems.push_back(qItem);
}

// Checks if the quest menu has a specific item
bool QuestMenu::HasItem(uint32 questid) const
{
    for (QuestMenuItemList::const_iterator i = m_qItems.begin(); i != m_qItems.end(); ++i)
    {
        if (i->m_qId == questid)
        {
            return true;
        }
    }
    return false;
}

// Clears all quest menu items
void QuestMenu::ClearMenu()
{
    m_qItems.clear();
}

// Sends the quest giver quest list to the player
void PlayerMenu::SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID)
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_LIST, 100); // guess size
    data << ObjectGuid(npcGUID);
    data << Title;
    data << uint32(eEmote._Delay); // player emote
    data << uint32(eEmote._Emote); // NPC emote

    size_t count_pos = data.wpos();
    data << uint8(mQuestMenu.MenuItemCount()); // TODO maximum 32 entries

    uint32 count = 0;
    for (count = 0; count < mQuestMenu.MenuItemCount(); ++count)
    {
        QuestMenuItem const& qmi = mQuestMenu.GetItem(count);

        uint32 questID = qmi.m_qId;

        if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID))
        {
            int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
            std::string title = pQuest->GetTitle();
            sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

            data << uint32(questID);
            data << uint32(qmi.m_qIcon);
            data << uint32(pQuest->GetQuestLevel());
            data << title;
        }
    }
    data.put<uint8>(count_pos, count);
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_LIST NPC Guid = %s", npcGUID.GetString().c_str());
}

// Sends the quest giver status to the player
void PlayerMenu::SendQuestGiverStatus(uint8 questStatus, ObjectGuid npcGUID) const
{
    WorldPacket data(SMSG_QUESTGIVER_STATUS, 12);
    data << ObjectGuid(npcGUID);
    data << uint32(questStatus);

    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_STATUS for %s", npcGUID.GetString().c_str());
}

// Sends the quest giver quest details to the player
void PlayerMenu::SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid guid, bool ActivateAccept) const
{
    // Retrieve the quest title, details, and objectives
    std::string Title      = pQuest->GetTitle();
    std::string Details    = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
        }
    }

    // Prepare the packet to send quest details
    WorldPacket data(SMSG_QUESTGIVER_QUEST_DETAILS, 100);   // guess size
    data << guid;
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << Details;
    data << Objectives;
    data << uint32(ActivateAccept ? 1 : 0);                 // auto finish

    // Handle hidden rewards flag
    if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
    {
        data << uint32(0);                                  // Rewarded chosen items hidden
        data << uint32(0);                                  // Rewarded items hidden
        data << uint32(0);                                  // Rewarded money hidden
    }
    else
    {
        // Add reward choice items
        ItemPrototype const* IProto;

        uint32 count = pQuest->GetRewChoiceItemsCount();
        data << uint32(count);

        for (uint32 i = 0; i < count; ++i)
        {
            data << uint32(pQuest->RewChoiceItemId[i]);
            data << uint32(pQuest->RewChoiceItemCount[i]);
            IProto = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]);
            if (IProto)
            {
                data << uint32(IProto->DisplayInfoID);
            }
            else
            {
                data << uint32(0x00);
            }
        }

        count = pQuest->GetRewItemsCount();
        data << uint32(count);

        for (uint32 i = 0; i < count; ++i)
        {
            data << uint32(pQuest->RewItemId[i]);
            data << uint32(pQuest->RewItemCount[i]);
            IProto = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]);
            if (IProto)
            {
                data << uint32(IProto->DisplayInfoID);
            }
            else
            {
                data << uint32(0x00);
            }
        }

        // Add reward or required money
        data << uint32(pQuest->GetRewOrReqMoney());
    }

    data << uint32(pQuest->GetRewSpell());

    uint32 count = pQuest->GetDetailsEmoteCount();
    data << uint32(count);
    for (uint32 i = 0; i < count; ++i)
    {
        data << uint32(pQuest->DetailsEmote[i]);
        data << uint32(pQuest->DetailsEmoteDelay[i]); // delay between emotes in ms
    }

    // Send the packet to the player
    GetMenuSession()->SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_DETAILS - for %s of %s, questid = %u", GetMenuSession()->GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), pQuest->GetQuestId());
}

// Sends the quest query response to the player
void PlayerMenu::SendQuestQueryResponse(Quest const* pQuest) const
{
    // Retrieve the quest title, details, objectives, and end text
    std::string Title = pQuest->GetTitle();
    std::string Details = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();
    std::string EndText = pQuest->GetEndText();
    std::string ObjectiveText[QUEST_OBJECTIVES_COUNT];

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ObjectiveText[i] = pQuest->ObjectiveText[i];
    }

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
            if (ql->EndText.size() > (size_t)loc_idx && !ql->EndText[loc_idx].empty())
            {
                EndText = ql->EndText[loc_idx];
            }

            for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                if (ql->ObjectiveText[i].size() > (size_t)loc_idx && !ql->ObjectiveText[i][loc_idx].empty())
                {
                    ObjectiveText[i] = ql->ObjectiveText[i][loc_idx];
                }
            }
        }
    }

    // Prepare the packet to send quest query response
    WorldPacket data(SMSG_QUEST_QUERY_RESPONSE, 100);       // guess size

    data << uint32(pQuest->GetQuestId());                   // quest id
    data << uint32(pQuest->GetQuestMethod());               // quest method
    data << uint32(pQuest->GetQuestLevel());                // quest level
    data << uint32(pQuest->GetZoneOrSort());                // zone or sort to display in quest log
    data << uint32(pQuest->GetType());                      // quest type
    //[-ZERO] data << uint32(pQuest->GetSuggestedPlayers());

    data << uint32(pQuest->GetRepObjectiveFaction());       // shown in quest log as part of quest objective
    data << uint32(pQuest->GetRepObjectiveValue());         // shown in quest log as part of quest objective

    data << uint32(0);                                      // RequiredOpositeRepFaction
    data << uint32(0);                                      // RequiredOpositeRepValue
    data << uint32(pQuest->GetNextQuestInChain());          // next quest in chain

    // Handle hidden rewards flag
    if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
    {
        data << uint32(0);                                  // Hide money rewarded
    }
    else
    {
        data << uint32(pQuest->GetRewOrReqMoney());         // reward money
    }

    data << uint32(pQuest->GetRewMoneyMaxLevel());          // used in XP calculation at client

    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)

    data << uint32(pQuest->GetSrcItemId());                 // source item id
    data << uint32(pQuest->GetQuestFlags());                // quest flags

    // Add reward items
    int iI;
    if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
    {
        for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
        {
            data << uint32(0) << uint32(0);
        }
        for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
        {
            data << uint32(0) << uint32(0);
        }
    }
    else
    {
        for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
        {
            data << uint32(pQuest->RewItemId[iI]);
            data << uint32(pQuest->RewItemCount[iI]);
        }
        for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
        {
            data << uint32(pQuest->RewChoiceItemId[iI]);
            data << uint32(pQuest->RewChoiceItemCount[iI]);
        }
    }

    // Add quest point information
    data << pQuest->GetPointMapId();
    data << pQuest->GetPointX();
    data << pQuest->GetPointY();
    data << pQuest->GetPointOpt();

    // Add quest texts
    data << Title;
    data << Objectives;
    data << Details;
    data << EndText;

    // Add quest objectives
    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
    {
        if (pQuest->ReqCreatureOrGOId[iI] < 0)
        {
            // client expected gameobject template id in form (id|0x80000000)
            data << uint32((pQuest->ReqCreatureOrGOId[iI] * (-1)) | 0x80000000);
        }
        else
        {
            data << uint32(pQuest->ReqCreatureOrGOId[iI]);
        }
        data << uint32(pQuest->ReqCreatureOrGOCount[iI]);
        data << uint32(pQuest->ReqItemId[iI]);
        data << uint32(pQuest->ReqItemCount[iI]);
    }

    // Add objective texts
    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
    {
        data << ObjectiveText[iI];
    }

    // Send the packet to the player
    GetMenuSession()->SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUEST_QUERY_RESPONSE questid=%u", pQuest->GetQuestId());
}

void PlayerMenu::SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) const
{
    // Retrieve the quest title and offer reward text
    std::string Title = pQuest->GetTitle();
    std::string OfferRewardText = pQuest->GetOfferRewardText();

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->OfferRewardText.size() > (size_t)loc_idx && !ql->OfferRewardText[loc_idx].empty())
            {
                OfferRewardText = ql->OfferRewardText[loc_idx];
            }
        }
    }

    // Prepare the packet to send quest offer reward
    WorldPacket data(SMSG_QUESTGIVER_OFFER_REWARD, 50);     // guess size

    // Add NPC GUID, quest ID, title, and offer reward text to the packet
    data << ObjectGuid(npcGUID);
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << OfferRewardText;

    // Add auto finish flag and suggested players count to the packet
    data << uint32(EnableNext ? 1 : 0);                     // Auto Finish

    // Add quest emotes to the packet
    uint32 EmoteCount = 0;
    for (uint32 i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        if (pQuest->OfferRewardEmote[i] <= 0)
        {
            break;
        }
        ++EmoteCount;
    }

    data << EmoteCount;                                     // Emote Count
    // TODO unify cycle constructions: the previous one allows non-sequential data placing, while the next one does not
    for (uint32 i = 0; i < EmoteCount; ++i)
    {
        data << uint32(pQuest->OfferRewardEmoteDelay[i]);   // Delay Emote
        data << uint32(pQuest->OfferRewardEmote[i]);
    }

    // Add reward choice items to the packet
    ItemPrototype const* pItem;
    data << uint32(pQuest->GetRewChoiceItemsCount());
    for (uint32 i = 0; i < pQuest->GetRewChoiceItemsCount(); ++i)
    {
        pItem = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]);

        data << uint32(pQuest->RewChoiceItemId[i]);
        data << uint32(pQuest->RewChoiceItemCount[i]);

        if (pItem)
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0x00);
        }
    }

    // Add reward items to the packet
    data << uint32(pQuest->GetRewItemsCount());
    for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
    {
        pItem = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemCount[i]);

        if (pItem)
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0x00);
        }
    }

    // Add reward or required money to the packet
    data << uint32(pQuest->GetRewOrReqMoney());

    data << uint32(pQuest->GetRewSpellCast());              // casted spell [-zero] to check
    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)

    GetMenuSession()->SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_OFFER_REWARD NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}

void PlayerMenu::SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const
{
    // We can always call to RequestItems, but this packet only goes out if there are actually
    // items.  Otherwise, we'll skip straight to the OfferReward

    std::string Title = pQuest->GetTitle();
    std::string RequestItemsText = pQuest->GetRequestItemsText();

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->RequestItemsText.size() > (size_t)loc_idx && !ql->RequestItemsText[loc_idx].empty())
            {
                RequestItemsText = ql->RequestItemsText[loc_idx];
            }
        }
    }

    // Quests that don't require items use the RequestItemsText field to store the text
    // that is shown when you talk to the quest giver while the quest is incomplete.
    // Therefore the text should not be shown for them when the quest is complete.
    // For quests that do require items, it is self explanatory.
    if (RequestItemsText.empty() || ((pQuest->GetReqItemsCount() == 0) && Completable))
    {
        SendQuestGiverOfferReward(pQuest, npcGUID, true);
        return;
    }

    // Prepare the packet to send quest request items
    WorldPacket data(SMSG_QUESTGIVER_REQUEST_ITEMS, 50);    // guess size
    data << ObjectGuid(npcGUID);                            // NPC GUID
    data << uint32(pQuest->GetQuestId());                   // Quest ID
    data << Title;                                          // Quest title
    data << RequestItemsText;                               // Request items text

    data << uint32(0x00);                                   // emote delay

    // Add the appropriate emote based on whether the quest is completable
    if (Completable)
    {
        data << pQuest->GetCompleteEmote();                 // emote id
    }
    else
    {
        data << pQuest->GetIncompleteEmote();
    }

    // Add the close on cancel flag
    if (CloseOnCancel)
    {
        data << uint32(0x01);                               // auto finish
    }
    else
    {
        data << uint32(0x00);
    }

    // Required Money
    data << uint32(pQuest->GetRewOrReqMoney() < 0 ? -pQuest->GetRewOrReqMoney() : 0);

    // Add the required items
    data << uint32(pQuest->GetReqItemsCount());
    ItemPrototype const* pItem;
    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (!pQuest->ReqItemId[i])
        {
            continue;
        }
        pItem = ObjectMgr::GetItemPrototype(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemCount[i]);

        if (pItem)
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0);
        }
    }

    data << uint32(0x02);

    if (!Completable)                                       // Completable = flags1 && flags2 && flags3 && flags4
    {
        data << uint32(0x00);                               // flags1
    }
    else
    {
        data << uint32(0x03);
    }

    data << uint32(0x04);                                   // flags2
    data << uint32(0x08);                                   // flags3
    //data << uint32(0x10);                                   // [-ZERO] flags4

    // Send the packet to the player
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_REQUEST_ITEMS NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}
