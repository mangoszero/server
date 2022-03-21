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

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "UpdateMask.h"
#include "Auth/md5.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Database/DatabaseImpl.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "Chat.h"
#include "SpellMgr.h"
#include "GameTime.h"
#include "Timer.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#endif

// config option SkipCinematics supported values
enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE      = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL       = 2
};

class LoginQueryHolder : public SqlQueryHolder
{
    private:
        uint32 m_accountId;
        ObjectGuid m_guid;
    public:
        LoginQueryHolder(uint32 accountId, ObjectGuid guid)
            : m_accountId(accountId), m_guid(guid) { }
        ObjectGuid GetGuid() const { return m_guid; }
        uint32 GetAccountId() const { return m_accountId; }
        bool Initialize();
};

#ifdef ENABLE_PLAYERBOTS
class PlayerbotLoginQueryHolder : public LoginQueryHolder
{
private:
    uint32 masterAccountId;
    PlayerbotHolder* playerbotHolder;

public:
    PlayerbotLoginQueryHolder(PlayerbotHolder* playerbotHolder, uint32 masterAccount, uint32 accountId, ObjectGuid guid)
        : LoginQueryHolder(accountId, guid), masterAccountId(masterAccount), playerbotHolder(playerbotHolder) { }

public:
    uint32 GetMasterAccountId() const { return masterAccountId; }
    PlayerbotHolder* GetPlayerbotHolder() { return playerbotHolder; }
};
#endif

bool LoginQueryHolder::Initialize()
{
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;

    // NOTE: all fields in `characters` must be read to prevent lost character data at next save in case wrong DB structure.
    // !!! NOTE: including unused `zone`,`online`
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,            "SELECT `guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `xp`, `money`, `playerBytes`, `playerBytes2`, `playerFlags`,"
                     "`position_x`, `position_y`, `position_z`, `map`, `orientation`, `taximask`, `cinematic`, `totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`,"
                     "`resettalents_time`, `trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, `online`, `death_expire_time`, `taxi_path`,"
                     "`honor_highest_rank`, `honor_standing`, `stored_honor_rating`, `stored_dishonorable_kills`, `stored_honorable_kills`,"
                     "`watchedFaction`, `drunk`,"
                     "`health`, `power1`, `power2`, `power3`, `power4`, `power5`, `exploredZones`, `equipmentCache`, `ammoId`, `actionBars`, `createdDate` FROM `characters` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,           "SELECT `groupId` FROM group_member WHERE `memberGuid` ='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,  "SELECT `id`, `permanent`, `map`, `resettime` FROM `character_instance` LEFT JOIN `instance` ON `instance` = `id` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,           "SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `character_aura` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,          "SELECT `spell`,`active`,`disabled` FROM `character_spell` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,     "SELECT `quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4` FROM `character_queststatus` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHONORCP,         "SELECT `victim_type`,`victim`,`honor`,`date`,`type` FROM `character_honor_cp` WHERE `used`=0 AND `guid`='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,      "SELECT `faction`,`standing`,`flags` FROM `character_reputation` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,       "SELECT `data`,`bag`,`slot`,`item`,`item_template` FROM `character_inventory` JOIN `item_instance` ON `character_inventory`.`item` = `item_instance`.`guid` WHERE `character_inventory`.`guid` = '%u' ORDER BY `bag`,`slot`", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,        "SELECT `guid`,`itemid`,`amount`,`property` FROM `item_loot` WHERE `owner_guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,         "SELECT `button`,`action`,`type` FROM `character_action` WHERE `guid` = '%u' ORDER BY `button`", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,      "SELECT `friend`,`flags` FROM `character_social` WHERE `guid` = '%u' LIMIT 255", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,        "SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,  "SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,           "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,          "SELECT `instance_id`, `team`, `join_x`, `join_y`, `join_z`, `join_o`, `join_map` FROM `character_battleground_data` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT `skill`, `value`, `max` FROM `character_skills` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,           "SELECT `id`,`messageType`,`sender`,`receiver`,`subject`,`itemTextId`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`,`stationery`,`mailTemplateId`,`has_items` FROM `mail` WHERE `receiver` = '%u' ORDER BY `id` DESC", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,     "SELECT `data`, `mail_id`, `item_guid`, `item_template` FROM `mail_items` JOIN `item_instance` ON `item_guid` = `guid` WHERE `receiver` = '%u'", m_guid.GetCounter());

    return res;
}

// don't call WorldSession directly
// it may get deleted before the query callbacks get executed
// instead pass an account id to this handler
class CharacterHandler
{
    public:
        void HandleCharEnumCallback(QueryResult* result, uint32 account)
        {
            WorldSession* session = sWorld.FindSession(account);
            if (!session)
            {
                delete result;
                return;
            }
            session->HandleCharEnum(result);
        }
        void HandlePlayerLoginCallback(QueryResult * /*dummy*/, SqlQueryHolder* holder)
        {
            if (!holder)
            {
                return;
            }
            WorldSession* session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
            if (!session)
            {
                delete holder;
                return;
            }
#ifdef ENABLE_PLAYERBOTS
            ObjectGuid guid = ((LoginQueryHolder*)holder)->GetGuid();
#endif
            session->HandlePlayerLogin((LoginQueryHolder*)holder);
#ifdef ENABLE_PLAYERBOTS
            Player* player = sObjectMgr.GetPlayer(guid, true);
            if (player && !player->GetPlayerbotAI())
            {
                player->SetPlayerbotMgr(new PlayerbotMgr(player));
                sRandomPlayerbotMgr.OnPlayerLogin(player);
            }
#endif
        }
#ifdef ENABLE_PLAYERBOTS
        void HandlePlayerBotLoginCallback(QueryResult * dummy, SqlQueryHolder * holder)
        {
            if (!holder)
            {
                return;
            }

            PlayerbotLoginQueryHolder* lqh = (PlayerbotLoginQueryHolder*)holder;
            if (sObjectMgr.GetPlayer(lqh->GetGuid()))
            {
                delete holder;
                return;
            }

            PlayerbotHolder* playerbotHolder = lqh->GetPlayerbotHolder();
            uint32 masterAccount = lqh->GetMasterAccountId();
            WorldSession* masterSession = masterAccount ? sWorld.FindSession(masterAccount) : NULL;

            // The bot's WorldSession is owned by the bot's Player object
            // The bot's WorldSession is deleted by PlayerbotMgr::LogoutPlayerBot
            uint32 botAccountId = lqh->GetAccountId();
            WorldSession *botSession = new WorldSession(botAccountId, NULL, SEC_PLAYER, 0, LOCALE_enUS);
            botSession->m_Address = "bot";
            botSession->HandlePlayerLogin(lqh); // will delete lqh
            Player* bot = botSession->GetPlayer();
            if (!bot)
            {
                return;
            }

            bool allowed = false;
            if (botAccountId == masterAccount)
            {
                allowed = true;
            }
            else if (masterSession && sPlayerbotAIConfig.allowGuildBots && bot->GetGuildId() == masterSession->GetPlayer()->GetGuildId())
            {
                allowed = true;
            }
            else if (sPlayerbotAIConfig.IsInRandomAccountList(botAccountId))
            {
                allowed = true;
            }

            if (allowed)
            {
                playerbotHolder->OnBotLogin(bot);
            }
            else if (masterSession)
            {
                ChatHandler ch(masterSession);
                ch.PSendSysMessage("You are not allowed to control bot %s...", bot->GetName());
                playerbotHolder->LogoutPlayerBot(bot->GetObjectGuid().GetRawValue());
            }
        }
#endif
} chrHandler;

#ifdef ENABLE_PLAYERBOTS
void PlayerbotHolder::AddPlayerBot(uint64 playerGuid, uint32 masterAccountId)
{
    // has bot already been added?
    if (sObjectMgr.GetPlayer(ObjectGuid(playerGuid)))
    {
        return;
    }

    uint32 accountId = sObjectMgr.GetPlayerAccountIdByGUID(ObjectGuid(playerGuid));
    if (accountId == 0)
    {
        return;
    }

    PlayerbotLoginQueryHolder *holder = new PlayerbotLoginQueryHolder(this, masterAccountId, accountId, ObjectGuid(playerGuid));
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        return;
    }
    CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerBotLoginCallback, holder);
}
#endif

void WorldSession::HandleCharEnum(QueryResult* result)
{
    WorldPacket data(SMSG_CHAR_ENUM, 100);                  // we guess size

    uint8 num = 0;

    data << num;

    if (result)
    {
        do
        {
            uint32 guidlow = (*result)[0].GetUInt32();
            DETAIL_LOG("Build enum data for char guid %u from account %u.", guidlow, GetAccountId());
            if (Player::BuildEnumData(result, &data))
            {
                ++num;
            }
        }
        while (result->NextRow());

        delete result;
    }

    data.put<uint8>(0, num);

    SendPacket(&data);
}

void WorldSession::HandleCharEnumOpcode(WorldPacket & /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their pets) on the account
    CharacterDatabase.AsyncPQuery(&chrHandler, &CharacterHandler::HandleCharEnumCallback, GetAccountId(),
                                  //           0               1                2                3                 4                  5                       6                        7
                                  "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "
                                  //   8                9               10                     11                     12                     13                    14
                                  "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "
                                  //  15                    16                   17                     18                   19
                                  "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache` "
                                  "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid`=`character_pet`.`owner` AND `character_pet`.`slot`='%u' "
                                  "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
                                  "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`",
                                  PET_SAVE_AS_CURRENT, GetAccountId());
}

void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
    std::string name;
    uint8 race_, class_;

    recv_data >> name;

    recv_data >> race_;
    recv_data >> class_;

    // extract other data required for player creating
    uint8 gender, skin, face, hairStyle, hairColor, facialHair, outfitId;
    recv_data >> gender >> skin >> face;
    recv_data >> hairStyle >> hairColor >> facialHair >> outfitId;

    WorldPacket data(SMSG_CHAR_CREATE, 1);                  // returned with diff.values in all cases

    if (GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(race_);
            switch (team)
            {
                case ALLIANCE: disabled = mask & (1 << 0); break;
                case HORDE:    disabled = mask & (1 << 1); break;
                default: break;
            }

            if (disabled)
            {
                data << (uint8)CHAR_CREATE_DISABLED;
                SendPacket(&data);
                return;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race_);

    if (!classEntry || !raceEntry)
    {
        data << (uint8)CHAR_CREATE_FAILED;
        SendPacket(&data);
        sLog.outError("Class: %u or Race %u not found in DBC (Wrong DBC files?) or Cheater?", class_, race_);
        return;
    }

    // prevent character creating with invalid name
    if (!normalizePlayerName(name))
    {
        data << (uint8)CHAR_NAME_NO_NAME;
        SendPacket(&data);
        sLog.outError("Account:[%d] but tried to Create character with empty [name]", GetAccountId());
        return;
    }

    // check name limitations
    uint8 res = ObjectMgr::CheckPlayerName(name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        data << uint8(res);
        SendPacket(&data);
        return;
    }

    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(name))
    {
        data << (uint8)CHAR_NAME_RESERVED;
        SendPacket(&data);
        return;
    }

    if (sObjectMgr.GetPlayerGuidByName(name))
    {
        data << (uint8)CHAR_CREATE_NAME_IN_USE;
        SendPacket(&data);
        return;
    }

    QueryResult* resultacct = LoginDatabase.PQuery("SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '%u'", GetAccountId());
    if (resultacct)
    {
        Field* fields = resultacct->Fetch();
        uint32 acctcharcount = fields[0].GetUInt32();
        delete resultacct;

        if (acctcharcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            data << (uint8)CHAR_CREATE_ACCOUNT_LIMIT;
            SendPacket(&data);
            return;
        }
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", GetAccountId());
    uint8 charcount = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        charcount = fields[0].GetUInt8();
        delete result;

        if (charcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            data << (uint8)CHAR_CREATE_SERVER_LIMIT;
            SendPacket(&data);
            return;
        }
    }

    bool AllowTwoSideAccounts = !sWorld.IsPvPRealm() || sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;
    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE)
    {
        QueryResult* result2 = CharacterDatabase.PQuery("SELECT `race` FROM `characters` WHERE `account` = '%u' %s",
                               GetAccountId(), (skipCinematics == CINEMATICS_SKIP_SAME_RACE) ? "" : "LIMIT 1");
        if (result2)
        {
            Team team_ = Player::TeamForRace(race_);

            Field* field = result2->Fetch();
            uint8 acc_race  = field[0].GetUInt32();

            // need to check team only for first character
            // TODO: what to if account already has characters of both races?
            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    data << (uint8)CHAR_CREATE_PVP_TEAMS_VIOLATION;
                    SendPacket(&data);
                    delete result2;
                    return;
                }
            }

            // search same race for cinematic or same class if need
            // TODO: check if cinematic already shown? (already logged in?; cinematic field)
            while (skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race)
            {
                if (!result2->NextRow())
                {
                    break;
                }

                field = result2->Fetch();
                acc_race = field[0].GetUInt32();

                have_same_race = race_ == acc_race;
            }
            delete result2;
        }
    }

    Player* pNewChar = new Player(this);
    // Sets the createdTime of the character which is UNIX timestamp
    uint32 createdDate = GetUnixTimeStamp(); // Unix Timestamp in seconds
    pNewChar->SetCreatedDate(createdDate); // TODO get currentTimeStamp for createdTime

    if (!pNewChar->Create(sObjectMgr.GeneratePlayerLowGuid(), name, race_, class_, gender, skin, face, hairStyle, hairColor, facialHair, outfitId))
    {
        // Player not create (race/class problem?)
        delete pNewChar;

        data << (uint8)CHAR_CREATE_ERROR;
        SendPacket(&data);

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) || skipCinematics == CINEMATICS_SKIP_ALL)
    {
        pNewChar->setCinematic(1);                           // not show intro
    }

    pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST);               // First login

    // Player created, save it now
    pNewChar->SaveToDB();
    charcount += 1;

    LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`= '%u' AND `realmid`= '%u'", GetAccountId(), realmID);
    LoginDatabase.PExecute("INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (%u, %u, %u)",  charcount, GetAccountId(), realmID);

    data << (uint8)CHAR_CREATE_SUCCESS;
    SendPacket(&data);

    std::string IP_str = GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());
    sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->OnCreate(pNewChar);
#endif /* ENABLE_ELUNA */

    delete pNewChar;                                        // created only to call SaveToDB()
}

void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't delete loaded character
    if (sObjectMgr.GetPlayer(guid))
    {
        return;
    }

    uint32 accountId = 0;
    std::string name;

    // is guild leader
    if (sGuildMgr.GetGuildByLeader(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << uint8(CHAR_DELETE_FAILED);
        SendPacket(&data);
        return;
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `account`,`name` FROM `characters` WHERE `guid`='%u'", lowguid);
    if (result)
    {
        Field* fields = result->Fetch();
        accountId = fields[0].GetUInt32();
        name = fields[1].GetCppString();
        delete result;
    }

    // prevent deleting other players' characters using cheating tools
    if (accountId != GetAccountId())
    {
        return;
    }

    std::string IP_str = GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);
    sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->OnDelete(lowguid);
#endif /* ENABLE_ELUNA */

    if (sLog.IsOutCharDump())                               // optimize GetPlayerDump call
    {
        std::string dump = PlayerDumpWriter().GetDump(lowguid);
        sLog.outCharDump(dump.c_str(), GetAccountId(), lowguid, name.c_str());
    }

    Player::DeleteFromDB(guid, GetAccountId());

    WorldPacket data(SMSG_CHAR_DELETE, 1);
    data << (uint8)CHAR_DELETE_SUCCESS;
    SendPacket(&data);
}

void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data)
{
    ObjectGuid playerGuid;
    recv_data >> playerGuid;

    if (PlayerLoading() || GetPlayer() != NULL)
    {
        sLog.outError("Player tryes to login again, AccountId = %d", GetAccountId());
        return;
    }

    m_playerLoading = true;

    DEBUG_LOG("WORLD: Received opcode Player Logon Message");

    LoginQueryHolder* holder = new LoginQueryHolder(GetAccountId(), playerGuid);
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerLoginCallback, holder);
}

void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
    /* Store the player's GUID for later reference */
    ObjectGuid playerGuid = holder->GetGuid();

    /* Create a new instance of the player object */
    Player* pCurrChar = new Player(this);

    /* Initialize a motion generator */
    pCurrChar->GetMotionMaster()->Initialize();

    /* Account ID is validated in LoadFromDB (prevents cheaters logging in to characters not on their account) */
    if (!pCurrChar->LoadFromDB(playerGuid, holder))         /// Could not load character from database, cancel login
    {
        /* Disconnect the game client */
        KickPlayer();

        /* Remove references to avoid dangling pointers */
        delete pCurrChar;
        delete holder;

        /* Checked in WorldSession::Update */
        m_playerLoading = false;

        return;
    }

    /* Validation check completely, assign player to WorldSession::_player for later use */
    SetPlayer(pCurrChar);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << pCurrChar->GetMapId();
    data << pCurrChar->GetPositionX();
    data << pCurrChar->GetPositionY();
    data << pCurrChar->GetPositionZ();
    data << pCurrChar->GetOrientation();
    SendPacket(&data);

    data.Initialize(SMSG_ACCOUNT_DATA_TIMES, 128);
    for (int i = 0; i < 32; ++i)
    {
        data << uint32(0);
    }
    SendPacket(&data);

    /* 1.12.1 does not have SMSG_MOTD, so we send a server message */
    /* Used for counting number of newlines in MOTD */

    // Send MOTD
    {
        uint32 linecount = 0;
        /* The MOTD itself */
        std::string str_motd = sWorld.GetMotd();
        /* Used for tracking our position within the MOTD while iterating through it */
        std::string::size_type pos = 0, nextpos;

        /* Find the next occurance of @ in the string
         * This is how newlines are represented */
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            /* If these are not equal, it means a '@' was found
             * These are used to represent newlines in the string
             * It is set by the code above here */
            if (nextpos != pos)
            {
                /* Send the player a system message containing the substring from pos to nextpos - pos */
                ChatHandler(pCurrChar).PSendSysMessage("%s", str_motd.substr(pos, nextpos - pos).c_str());
                ++linecount;
            }
            pos = nextpos + 1;
        }
        /* There are no more newlines in our MOTD, so we send whatever is left */
        if (pos < str_motd.length())
        {
            ChatHandler(pCurrChar).PSendSysMessage("%s", str_motd.substr(pos).c_str());
        }
        DEBUG_LOG("WORLD: Sent motd (SMSG_MOTD)");
    }

    /* Attempt to load guild for player */
    if (QueryResult *resultGuild = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD))
    {
        /* We're in a guild, so set the player's guild data to represent that */
        Field* fields = resultGuild->Fetch();
        pCurrChar->SetInGuild(fields[0].GetUInt32());
        pCurrChar->SetRank(fields[1].GetUInt32());
        /* Avoid dangling pointers */
        delete resultGuild;
    }
    /* Player thinks they have a guild, but it isn't in the database. Clear that information */
    else if (pCurrChar->GetGuildId())
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetRank(0);
    }

    /* Player is in a guild
     * TODO: Can we move this code into the block above? Not sure why it's down here */
    if (pCurrChar->GetGuildId() != 0)
    {
        /* Get guild based on what we set the player's guild to above */
        Guild* guild = sGuildMgr.GetGuildById(pCurrChar->GetGuildId());

        /* More checks to see if they're in a guild? I'm sure this is redundant */
        if (guild)
        {
            /* Build MOTD packet and send it to the player */
            data.Initialize(SMSG_GUILD_EVENT, (1 + 1 + guild->GetMOTD().size() + 1));
            data << uint8(GE_MOTD);
            data << uint8(1);
            data << guild->GetMOTD();
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent guild-motd (SMSG_GUILD_EVENT)");

            /* Let everyone in the guild know you've just signed in */
            guild->BroadcastEvent(GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }
        /* If the player is not in a guild */
        else
        {
            sLog.outError("Player %s (GUID: %u) marked as member of nonexistent guild (id: %u), removing guild membership for player.",
                          pCurrChar->GetName(),
                          pCurrChar->GetGUIDLow(),
                          pCurrChar->GetGuildId());

            /* Set guild to 0 (again) */
            pCurrChar->SetInGuild(0);
        }
    }

    /* Don't let the player get stuck logging in with no corpse */
    if (!pCurrChar->IsAlive())
    {
        pCurrChar->SendCorpseReclaimDelay(true);
    }

    /* Sends information required before the player can be added to the map
     * TODO: See if we can send information about game objects here (prevent alt+f4 through object) */
    pCurrChar->SendInitialPacketsBeforeAddToMap();

    /* If it's the player's first login, send a cinematic */
    if (!pCurrChar->getCinematic())
    {
        pCurrChar->setCinematic(1);

        /* Set the start location to the player's racial starting point */
        if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(pCurrChar->getRace()))
        {
            pCurrChar->SendCinematicStart(rEntry->CinematicSequence);
        }
    }

    uint32 miscRequirement = 0;
    AreaLockStatus lockStatus = AREA_LOCKSTATUS_OK;
    if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(pCurrChar->GetMapId()))
    {
        lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, miscRequirement);
    }
    else
    {
        // Some basic checks in case of a map without areatrigger
        MapEntry const* mapEntry = sMapStore.LookupEntry(pCurrChar->GetMapId());
        if (!mapEntry)
        {
            lockStatus = AREA_LOCKSTATUS_UNKNOWN_ERROR;
        }
    }

    /* This code is run if we can not add the player to the map for some reason */
    if (lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->GetMap()->Add(pCurrChar))
    {
        /* Attempt to find an areatrigger to teleport the player for us */
        AreaTrigger const* at = sObjectMgr.GetGoBackTrigger(pCurrChar->GetMapId());
        if (at)
        {
            lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, miscRequirement);
        }

        /* We couldn't find an areatrigger to teleport, so just move the player back to their home bind */
        if (!at || lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, pCurrChar->GetOrientation()))
        {
            pCurrChar->TeleportToHomebind();
        }
    }

    sObjectAccessor.AddObject(pCurrChar);
    DEBUG_LOG("Player %s added to map %i", pCurrChar->GetName(), pCurrChar->GetMapId());

    /* send the player's social lists */
    pCurrChar->GetSocial()->SendFriendList();
    pCurrChar->GetSocial()->SendIgnoreList();

    /* Send packets that must be sent only after player is added to the map */
    pCurrChar->SendInitialPacketsAfterAddToMap();

    /* Mark player as online in the database */
    static SqlStatementID updChars;
    static SqlStatementID updAccount;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 1 WHERE `guid` = ?");
    stmt.PExecute(pCurrChar->GetGUIDLow());

#ifdef ENABLE_PLAYERBOTS
    if (pCurrChar->GetSession()->GetRemoteAddress() != "bot")
    {
#endif
        stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
        stmt.PExecute(realmID, GetAccountId());
#ifdef ENABLE_PLAYERBOTS
    }
#endif

    /* Sync player's in-game time with server time */
    pCurrChar->SetInGameTime(GameTime::GetGameTimeMS());

    /* Send logon notification to player's group
     * This is sent after player is added to the world so that player receives it too */
    if (Group* group = pCurrChar->GetGroup())
    {
        group->SendUpdate();
    }

    /* Inform player's friends that player has come online */
    sSocialMgr.SendFriendStatus(pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    /* Load the player's corpse if it exists, or resurrect the player if not */
    pCurrChar->LoadCorpse();

    /* If the player is dead, we need to set them as a ghost and increase movespeed */
    if (pCurrChar->m_deathState != ALIVE)
    {
        /* If player is a night elf, wisp racial should be applied */
        if (pCurrChar->getRace() == RACE_NIGHTELF)
        {
            pCurrChar->CastSpell(pCurrChar, 20584, true);   // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form), SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp form), SPELL_AURA_TRANSFORM (to wisp form)
        }

        /* Apply ghost spell to player */
        pCurrChar->CastSpell(pCurrChar, 8326, true);        // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?), SPELL_AURA_INCREASE_SWIM_SPEED(why?)

        /* Allow player to walk on water */
        pCurrChar->SetWaterWalk(true);
    }

    /* If player is on a taxi, continue their flight */
    pCurrChar->ContinueTaxiFlight();

    /* Load pet if player has one
     * If the player is dead or on a taxi, the pet will be remembered as a temporary summon */
    pCurrChar->LoadPet();

    /* If we're running an FFA PvP realm and the player isn't a GM, mark them as PvP flagged */
    if (sWorld.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        pCurrChar->SetFFAPvP(true);
    }

    if (pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
    {
        pCurrChar->SetContestedPvP();
    }

    /* Apply onLogon requests (such as talent resets) */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true);
        SendNotification(LANG_RESET_TALENTS);               // we can use SMSG_TALENTS_INVOLUNTARILY_RESET here
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
    {
        sEluna->OnFirstLogin(pCurrChar);
    }
#endif /* ENABLE_ELUNA */


    /* We've done what we need to, remove the flag */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
    {
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);
    }

    /* If the server is shutting down, show shutdown time remaining */
    if (sWorld.IsShutdowning())
    {
        sWorld.ShutdownMsg(true, pCurrChar);
    }

    /* If player should have all taxi paths, give them to the player */
    if (sWorld.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
    {
        pCurrChar->SetTaxiCheater(true);
    }

    /* Send GM notifications */
    if (pCurrChar->isGameMaster())
    {
        SendNotification(LANG_GM_ON);
    }

    if (!pCurrChar->isGMVisible())
    {
        SendNotification(LANG_INVISIBLE_INVISIBLE);
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
        if (invisibleAuraInfo && IsSpellAppliesAura(invisibleAuraInfo))
        {
            pCurrChar->CastSpell(pCurrChar, invisibleAuraInfo, true);
        }
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s) Login Character:[%s] (guid: %u)",
                 GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUIDLow());

    /* Make player stand up if they're not already stood up and not stunned */
    if (!pCurrChar->IsStandState() && !pCurrChar->hasUnitState(UNIT_STAT_STUNNED))
    {
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);
    }

    m_playerLoading = false;

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->OnLogin(pCurrChar);
#endif /* ENABLE_ELUNA */

    /* Used for movement */
    m_clientTimeDelay = 0;

    /* Used for looting */
    pCurrChar->lastTimeLooted = time(NULL);

    delete holder;
}

void WorldSession::HandleSetFactionAtWarOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    GetPlayer()->GetReputationMgr().SetAtWar(repListID, flag);
}

void WorldSession::HandleTutorialFlagOpcode(WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // sLog.outError("CHEATER? Account:[%d] Guid[%u] tried to send wrong CMSG_TUTORIAL_FLAG", GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    SetTutorialInt(wInt, tutflag);

    // DEBUG_LOG("Received Tutorial Flag Set {%u}.", iFlag);
}

void WorldSession::HandleTutorialClearOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0xFFFFFFFF);
    }
}

void WorldSession::HandleTutorialResetOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0x00000000);
    }
}

void WorldSession::HandleSetWatchedFactionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

void WorldSession::HandleSetFactionInactiveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    _player->GetReputationMgr().SetInactive(replistid, inactive);
}

void WorldSession::HandleShowingHelmOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

void WorldSession::HandleShowingCloakOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

void WorldSession::HandleCharRenameOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;

    recv_data >> guid;
    recv_data >> newname;

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_NO_NAME);
        SendPacket(&data);
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(res);
        SendPacket(&data);
        return;
    }

    // check name limitations
    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_RESERVED);
        SendPacket(&data);
        return;
    }

    std::string escaped_newname = newname;
    CharacterDatabase.escape_string(escaped_newname);

    // make sure that the character belongs to the current account, that rename at login is enabled
    // and that there is no character with the desired new name
    CharacterDatabase.AsyncPQuery(&WorldSession::HandleChangePlayerNameOpcodeCallBack,
                                  GetAccountId(), newname,
                                  "SELECT `guid`, `name` FROM `characters` WHERE `guid` = %u AND `account` = %u AND (`at_login` & %u) = %u AND NOT EXISTS (SELECT NULL FROM `characters` WHERE `name` = '%s')",
                                  guid.GetCounter(), GetAccountId(), AT_LOGIN_RENAME, AT_LOGIN_RENAME, escaped_newname.c_str()
                                 );
}

void WorldSession::HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId, std::string newname)
{
    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        delete result;
        return;
    }

    if (!result)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    uint32 guidLow = result->Fetch()[0].GetUInt32();
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    std::string oldname = result->Fetch()[1].GetCppString();

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE `characters` SET `name` = '%s', `at_login` = `at_login` & ~ %u WHERE `guid` ='%u'", newname.c_str(), uint32(AT_LOGIN_RENAME), guidLow);
    CharacterDatabase.CommitTransaction();

    sLog.outChar("Account: %d (IP: %s) Character:[%s] (guid:%u) Changed name to: %s", session->GetAccountId(), session->GetRemoteAddress().c_str(), oldname.c_str(), guidLow, newname.c_str());

    WorldPacket data(SMSG_CHAR_RENAME, 1 + 8 + (newname.size() + 1));
    data << uint8(RESPONSE_SUCCESS);
    data << guid;
    data << newname;
    session->SendPacket(&data);

    sWorld.InvalidatePlayerDataToAllClient(guid);
}
