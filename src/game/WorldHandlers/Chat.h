/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOSSERVER_CHAT_H
#define MANGOSSERVER_CHAT_H

#include "Common.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"

struct AreaTrigger;
struct AreaTriggerEntry;
struct FactionEntry;
struct FactionState;
struct GameTele;
struct SpellEntry;

class QueryResult;
class ChatHandler;
class WorldSession;
class WorldPacket;
class GMTicket;
class MailDraft;
class Object;
class GameObject;
class Creature;
class Player;
class Unit;

class ChatCommand
{
    public:
        const char*        Name;
        uint32             SecurityLevel;                   // function pointer required correct align (use uint32)
        bool               AllowConsole;
        bool (ChatHandler::*Handler)(char* args);
        std::string        Help;
        ChatCommand*       ChildCommands;
};

enum ChatCommandSearchResult
{
    CHAT_COMMAND_OK,                                        // found accessible command by command string
    CHAT_COMMAND_UNKNOWN,                                   // first level command not found
    CHAT_COMMAND_UNKNOWN_SUBCOMMAND,                        // command found but some level subcommand not find in subcommand list
};

enum PlayerChatTag
{
    CHAT_TAG_NONE               = 0,
    CHAT_TAG_AFK                = 1,
    CHAT_TAG_DND                = 2,
    CHAT_TAG_GM                 = 3,
};
typedef uint32 ChatTagFlags;

class ChatHandler
{
    public:
        explicit ChatHandler(WorldSession* session);
        explicit ChatHandler(Player* player);
        virtual ~ChatHandler();

        static char* LineFromMessage(char*& pos) { char* start = strtok(pos, "\n"); pos = NULL; return start; }

        // function with different implementation for chat/console
        virtual const char* GetMangosString(int32 entry) const;
        const char* GetOnOffStr(bool value) const;

        virtual void SendSysMessage(const char* str);

        void SendSysMessage(int32     entry);
        void PSendSysMessage(const char* format, ...) ATTR_PRINTF(2, 3);
        void PSendSysMessage(int32     entry, ...);
        void PSendSysMessageMultiline(int32 entry, ...);

        bool ParseCommands(const char* text);
        ChatCommand const* FindCommand(char const* text);

        bool isValidChatMessage(const char* msg);
        bool HasSentErrorMessage() { return sentErrorMessage;}

        /**
        * \brief Prepare SMSG_GM_MESSAGECHAT/SMSG_MESSAGECHAT
        *
        * Method:    BuildChatPacket build message chat packet generic way
        * FullName:  ChatHandler::BuildChatPacket
        * Access:    public static
        * Returns:   void
        *
        * \param WorldPacket& data             : Provided packet will be filled with requested info
        * \param ChatMsg msgtype               : Message type from ChatMsg enum from SharedDefines.h
        * \param ChatTagFlags chatTag          : Chat tag from PlayerChatTag in Chat.h
        * \param char const* message           : Message to send
        * \param Language language             : Language from Language enum in SharedDefines.h
        * \param ObjectGuid const& senderGuid  : May be null in some case but often required for ignore list
        * \param char const* senderName        : Required for type *MONSTER* or *BATTLENET, but also if GM is true
        * \param ObjectGuid const& targetGuid  : Often null, but needed for type *MONSTER* or *BATTLENET or *BATTLEGROUND* or *ACHIEVEMENT
        * \param char const* targetName        : Often null, but needed for type *MONSTER* or *BATTLENET or *BATTLEGROUND*
        * \param char const* channelName       : Required only for CHAT_MSG_CHANNEL
        * \param uint8 playerRank              : Used only for Defensive Channels (Value over 0 will show rank name before character name in channel)
        **/
        static void BuildChatPacket(
            WorldPacket& data, ChatMsg msgtype, char const* message, Language language = LANG_UNIVERSAL, ChatTagFlags chatTag = CHAT_TAG_NONE,
            ObjectGuid const& senderGuid = ObjectGuid(), char const* senderName = NULL,
            ObjectGuid const& targetGuid = ObjectGuid(), char const* targetName = NULL,
            char const* channelName = NULL, uint8 playerRank = 0);
    protected:
        explicit ChatHandler() : m_session(NULL) {}      // for CLI subclass

        bool hasStringAbbr(const char* name, const char* part);

        // function with different implementation for chat/console
        virtual uint32 GetAccountId() const;
        virtual AccountTypes GetAccessLevel() const;
        virtual bool isAvailable(ChatCommand const& cmd) const;
        virtual std::string GetNameLink() const;
        virtual bool needReportToTarget(Player* chr) const;
        virtual LocaleConstant GetSessionDbcLocale() const;
        virtual int GetSessionDbLocaleIndex() const;

        bool HasLowerSecurity(Player* target, ObjectGuid guid = ObjectGuid(), bool strong = false);
        bool HasLowerSecurityAccount(WorldSession* target, uint32 account, bool strong = false);

        void SendGlobalSysMessage(const char* str, AccountTypes minSec = SEC_PLAYER);

        bool SetDataForCommandInTable(ChatCommand* table, const char* text, uint32 security, std::string const& help);
        void ExecuteCommand(const char* text);
        void LogCommand(char const* fullcmd);

        bool ShowHelpForCommand(ChatCommand* table, const char* cmd);
        bool ShowHelpForSubCommands(ChatCommand* table, char const* cmd);
        ChatCommandSearchResult FindCommand(ChatCommand* table, char const*& text, ChatCommand*& command, ChatCommand** parentCommand = NULL, std::string* cmdNamePtr = NULL, bool allAvailable = false, bool exactlyName = false);

        void CheckIntegrity(ChatCommand* table, ChatCommand* parentCommand);
        ChatCommand* getCommandTable();

        bool HandleAccountCommand(char* args);
        bool HandleAccountCharactersCommand(char* args);
        bool HandleAccountCreateCommand(char* args);
        bool HandleAccountDeleteCommand(char* args);
        bool HandleAccountLockCommand(char* args);
        bool HandleAccountOnlineListCommand(char* args);
        bool HandleAccountPasswordCommand(char* args);
        bool HandleAccountSetAddonCommand(char* args);
        bool HandleAccountSetGmLevelCommand(char* args);
        bool HandleAccountSetPasswordCommand(char* args);

        bool HandleAHBotItemsAmountCommand(char* args);
        template <int Q>
        bool HandleAHBotItemsAmountQualityCommand(char* args);
        bool HandleAHBotItemsRatioCommand(char* args);
        template <int H>
        bool HandleAHBotItemsRatioHouseCommand(char* args);
        bool HandleAHBotRebuildCommand(char* args);
        bool HandleAHBotReloadCommand(char* args);
        bool HandleAHBotStatusCommand(char* args);

        bool HandleAuctionAllianceCommand(char* args);
        bool HandleAuctionGoblinCommand(char* args);
        bool HandleAuctionHordeCommand(char* args);
        bool HandleAuctionItemCommand(char* args);
        bool HandleAuctionCommand(char* args);

        bool HandleBanAccountCommand(char* args);
        bool HandleBanCharacterCommand(char* args);
        bool HandleBanIPCommand(char* args);
        bool HandleBanInfoAccountCommand(char* args);
        bool HandleBanInfoCharacterCommand(char* args);
        bool HandleBanInfoIPCommand(char* args);
        bool HandleBanListAccountCommand(char* args);
        bool HandleBanListCharacterCommand(char* args);
        bool HandleBanListIPCommand(char* args);

        bool HandleCastCommand(char* args);
        bool HandleCastBackCommand(char* args);
        bool HandleCastDistCommand(char* args);
        bool HandleCastSelfCommand(char* args);
        bool HandleCastTargetCommand(char* args);

        bool HandleCharacterDeletedDeleteCommand(char* args);
        bool HandleCharacterDeletedListCommand(char* args);
        bool HandleCharacterDeletedRestoreCommand(char* args);
        bool HandleCharacterDeletedOldCommand(char* args);
        bool HandleCharacterEraseCommand(char* args);
        bool HandleCharacterLevelCommand(char* args);
        bool HandleCharacterRenameCommand(char* args);
        bool HandleCharacterReputationCommand(char* args);

        bool HandleDebugAnimCommand(char* args);
        bool HandleDebugBattlegroundCommand(char* args);
        bool HandleDebugGetItemStateCommand(char* args);
        bool HandleDebugGetItemValueCommand(char* args);
        bool HandleDebugGetLootRecipientCommand(char* args);
        bool HandleDebugGetValueCommand(char* args);
        bool HandleDebugModItemValueCommand(char* args);
        bool HandleDebugModValueCommand(char* args);
        bool HandleDebugSetAuraStateCommand(char* args);
        bool HandleDebugSetItemValueCommand(char* args);
        bool HandleDebugSetValueCommand(char* args);
        bool HandleDebugSpellCheckCommand(char* args);
        bool HandleDebugSpellCoefsCommand(char* args);
        bool HandleDebugSpellModsCommand(char* args);
        bool HandleDebugUpdateWorldStateCommand(char* args);

        bool HandleDebugPlayCinematicCommand(char* args);
        bool HandleDebugPlaySoundCommand(char* args);

        bool HandleDebugRecvOpcodeCommand(char* args);
        bool HandleDebugSendBuyErrorCommand(char* args);
        bool HandleDebugSendChannelNotifyCommand(char* args);
        bool HandleDebugSendChatMsgCommand(char* args);
        bool HandleDebugSendEquipErrorCommand(char* args);
        bool HandleDebugSendOpcodeCommand(char* args);
        bool HandleDebugSendPoiCommand(char* args);
        bool HandleDebugSendQuestPartyMsgCommand(char* args);
        bool HandleDebugSendQuestInvalidMsgCommand(char* args);
        bool HandleDebugSendSellErrorCommand(char* args);
        bool HandleDebugSendSpellFailCommand(char* args);

        bool HandleEventListCommand(char* args);
        bool HandleEventStartCommand(char* args);
        bool HandleEventStopCommand(char* args);
        bool HandleEventInfoCommand(char* args);

        bool HandleGameObjectAddCommand(char* args);
        bool HandleGameObjectAnimationCommand(char* args);
        bool HandleGameObjectDeleteCommand(char* args);
        bool HandleGameObjectLootstateCommand(char* args);
        bool HandleGameObjectMoveCommand(char* args);
        bool HandleGameObjectNearCommand(char* args);
        bool HandleGameObjectPhaseCommand(char* args);
        bool HandleGameObjectStateCommand(char* args);
        bool HandleGameObjectTargetCommand(char* args);
        bool HandleGameObjectTurnCommand(char* args);

        bool HandleGMCommand(char* args);
        bool HandleGMChatCommand(char* args);
        bool HandleGMFlyCommand(char* args);
        bool HandleGMListFullCommand(char* args);
        bool HandleGMListIngameCommand(char* args);
        bool HandleGMVisibleCommand(char* args);

        bool HandleGoCommand(char* args);
        bool HandleGoCreatureCommand(char* args);
        bool HandleGoGraveyardCommand(char* args);
        bool HandleGoGridCommand(char* args);
        bool HandleGoObjectCommand(char* args);
        bool HandleGoTaxinodeCommand(char* args);
        bool HandleGoTriggerCommand(char* args);
        bool HandleGoXYCommand(char* args);
        bool HandleGoXYZCommand(char* args);
        bool HandleGoZoneXYCommand(char* args);

        bool HandleGuildCreateCommand(char* args);
        bool HandleGuildInviteCommand(char* args);
        bool HandleGuildUninviteCommand(char* args);
        bool HandleGuildRankCommand(char* args);
        bool HandleGuildDeleteCommand(char* args);

        bool HandleHonorShow(char* args);
        bool HandleHonorAddCommand(char* args);
        bool HandleHonorAddKillCommand(char* args);
        bool HandleHonorUpdateCommand(char* args);

        bool HandleInstanceListBindsCommand(char* args);
        bool HandleInstanceUnbindCommand(char* args);
        bool HandleInstanceStatsCommand(char* args);
        bool HandleInstanceSaveDataCommand(char* args);

        bool HandleLearnCommand(char* args);
        bool HandleLearnAllCommand(char* args);
        bool HandleLearnAllGMCommand(char* args);
        bool HandleLearnAllCraftsCommand(char* args);
        bool HandleLearnAllRecipesCommand(char* args);
        bool HandleLearnAllDefaultCommand(char* args);
        bool HandleLearnAllLangCommand(char* args);
        bool HandleLearnAllMyClassCommand(char* args);
        bool HandleLearnAllMySpellsCommand(char* args);
        bool HandleLearnAllMyTalentsCommand(char* args);

        bool HandleListAurasCommand(char* args);
        bool HandleListCreatureCommand(char* args);
        bool HandleListItemCommand(char* args);
        bool HandleListObjectCommand(char* args);
        bool HandleListTalentsCommand(char* args);

        bool HandleLookupAccountEmailCommand(char* args);
        bool HandleLookupAccountIpCommand(char* args);
        bool HandleLookupAccountNameCommand(char* args);
        bool HandleLookupAreaCommand(char* args);
        bool HandleLookupCreatureCommand(char* args);
        bool HandleLookupEventCommand(char* args);
        bool HandleLookupFactionCommand(char* args);
        bool HandleLookupItemCommand(char* args);
        bool HandleLookupItemSetCommand(char* args);
        bool HandleLookupObjectCommand(char* args);
        bool HandleLookupPlayerIpCommand(char* args);
        bool HandleLookupPlayerAccountCommand(char* args);
        bool HandleLookupPlayerEmailCommand(char* args);
        bool HandleLookupPoolCommand(char* args);
        bool HandleLookupQuestCommand(char* args);
        bool HandleLookupSkillCommand(char* args);
        bool HandleLookupSpellCommand(char* args);
        bool HandleLookupTaxiNodeCommand(char* args);
        bool HandleLookupTeleCommand(char* args);

        bool HandleModifyHPCommand(char* args);
        bool HandleModifyManaCommand(char* args);
        bool HandleModifyRageCommand(char* args);
        bool HandleModifyEnergyCommand(char* args);
        bool HandleModifyMoneyCommand(char* args);
        bool HandleModifyASpeedCommand(char* args);
        bool HandleModifySpeedCommand(char* args);
        bool HandleModifyBWalkCommand(char* args);
        bool HandleModifySwimCommand(char* args);
        bool HandleModifyScaleCommand(char* args);
        bool HandleModifyMountCommand(char* args);
        bool HandleModifyFactionCommand(char* args);
        bool HandleModifyTalentCommand(char* args);
        bool HandleModifyHonorCommand(char* args);
        bool HandleModifyRepCommand(char* args);
        bool HandleModifyGenderCommand(char* args);

        //-----------------------Npc Commands-----------------------
        bool HandleNpcAddCommand(char* args);
        bool HandleNpcAddVendorItemCommand(char* args);
        bool HandleNpcAIInfoCommand(char* args);
        bool HandleNpcAllowMovementCommand(char* args);
        bool HandleNpcChangeEntryCommand(char* args);
        bool HandleNpcChangeLevelCommand(char* args);
        bool HandleNpcDeleteCommand(char* args);
        bool HandleNpcDelVendorItemCommand(char* args);
        bool HandleNpcFactionIdCommand(char* args);
        bool HandleNpcFlagCommand(char* args);
        bool HandleNpcFollowCommand(char* args);
        bool HandleNpcInfoCommand(char* args);
        bool HandleNpcMoveCommand(char* args);
        bool HandleNpcPlayEmoteCommand(char* args);
        bool HandleNpcSayCommand(char* args);
        bool HandleNpcSetDeathStateCommand(char* args);
        bool HandleNpcSetModelCommand(char* args);
        bool HandleNpcSetMoveTypeCommand(char* args);
        bool HandleNpcSpawnDistCommand(char* args);
        bool HandleNpcSpawnTimeCommand(char* args);
        bool HandleNpcTameCommand(char* args);
        bool HandleNpcTextEmoteCommand(char* args);
        bool HandleNpcUnFollowCommand(char* args);
        bool HandleNpcWhisperCommand(char* args);
        bool HandleNpcYellCommand(char* args);

        // TODO: NpcCommands that needs to be fixed :
        bool HandleNpcAddWeaponCommand(char* args);
        bool HandleNpcNameCommand(char* args);
        bool HandleNpcSubNameCommand(char* args);
        //----------------------------------------------------------

        bool HandlePDumpLoadCommand(char* args);
        bool HandlePDumpWriteCommand(char* args);

        bool HandlePoolListCommand(char* args);
        bool HandlePoolSpawnsCommand(char* args);
        bool HandlePoolInfoCommand(char* args);

        bool HandleQuestAddCommand(char* args);
        bool HandleQuestRemoveCommand(char* args);
        bool HandleQuestCompleteCommand(char* args);

        bool HandleReloadAllCommand(char* args);
        bool HandleReloadAllAreaCommand(char* args);
        bool HandleReloadAllGossipsCommand(char* args);
        bool HandleReloadAllItemCommand(char* args);
        bool HandleReloadAllLootCommand(char* args);
        bool HandleReloadAllNpcCommand(char* args);
        bool HandleReloadAllQuestCommand(char* args);
        bool HandleReloadAllScriptsCommand(char* args);
        bool HandleReloadAllEventAICommand(char* args);
        bool HandleReloadAllSpellCommand(char* args);
        bool HandleReloadAllLocalesCommand(char* args);

        bool HandleReloadConfigCommand(char* args);

        bool HandleReloadAreaTriggerTavernCommand(char* args);
        bool HandleReloadAreaTriggerTeleportCommand(char* args);
        bool HandleReloadAutoBroadcastCommand(char* args);
        bool HandleReloadBattleEventCommand(char* args);
        bool HandleReloadCommandCommand(char* args);
        bool HandleReloadConditionsCommand(char* args);
        bool HandleReloadCreatureQuestRelationsCommand(char* args);
        bool HandleReloadCreatureQuestInvRelationsCommand(char* args);
        bool HandleReloadCreaturesStatsCommand(char* args);
        bool HandleReloadDbScriptStringCommand(char* args);
        bool HandleReloadDBScriptsOnCreatureDeathCommand(char* args);
        bool HandleReloadDBScriptsOnEventCommand(char* args);
        bool HandleReloadDBScriptsOnGossipCommand(char* args);
        bool HandleReloadDBScriptsOnGoUseCommand(char* args);
        bool HandleReloadDBScriptsOnQuestEndCommand(char* args);
        bool HandleReloadDBScriptsOnQuestStartCommand(char* args);
        bool HandleReloadDBScriptsOnSpellCommand(char* args);

        bool HandleReloadEventAITextsCommand(char* args);
        bool HandleReloadEventAISummonsCommand(char* args);
        bool HandleReloadEventAIScriptsCommand(char* args);
        bool HandleReloadGameGraveyardZoneCommand(char* args);
        bool HandleReloadGameTeleCommand(char* args);
        bool HandleReloadGossipMenuCommand(char* args);
        bool HandleReloadGOQuestRelationsCommand(char* args);
        bool HandleReloadGOQuestInvRelationsCommand(char* args);
        bool HandleReloadItemEnchantementsCommand(char* args);
        bool HandleReloadItemRequiredTragetCommand(char* args);
        bool HandleReloadLocalesCreatureCommand(char* args);
        bool HandleReloadLocalesGameobjectCommand(char* args);
        bool HandleReloadLocalesGossipMenuOptionCommand(char* args);
        bool HandleReloadLocalesItemCommand(char* args);
        bool HandleReloadLocalesNpcTextCommand(char* args);
        bool HandleReloadLocalesPageTextCommand(char* args);
        bool HandleReloadLocalesPointsOfInterestCommand(char* args);
        bool HandleReloadLocalesQuestCommand(char* args);
        bool HandleReloadLootTemplatesCreatureCommand(char* args);
        bool HandleReloadLootTemplatesDisenchantCommand(char* args);
        bool HandleReloadLootTemplatesFishingCommand(char* args);
        bool HandleReloadLootTemplatesGameobjectCommand(char* args);
        bool HandleReloadLootTemplatesItemCommand(char* args);
        bool HandleReloadLootTemplatesMailCommand(char* args);
        bool HandleReloadLootTemplatesPickpocketingCommand(char* args);
        bool HandleReloadLootTemplatesReferenceCommand(char* args);
        bool HandleReloadLootTemplatesSkinningCommand(char* args);
        bool HandleReloadMangosStringCommand(char* args);
        bool HandleReloadNpcGossipCommand(char* args);
        bool HandleReloadNpcTextCommand(char* args);
        bool HandleReloadNpcTrainerCommand(char* args);
        bool HandleReloadNpcVendorCommand(char* args);
        bool HandleReloadPageTextsCommand(char* args);
        bool HandleReloadPointsOfInterestCommand(char* args);
        bool HandleReloadQuestAreaTriggersCommand(char* args);
        bool HandleReloadQuestTemplateCommand(char* args);
        bool HandleReloadReservedNameCommand(char* args);
        bool HandleReloadReputationRewardRateCommand(char* args);
        bool HandleReloadReputationSpilloverTemplateCommand(char* args);
        bool HandleReloadScriptBindingCommand(char* args);
        bool HandleReloadSkillFishingBaseLevelCommand(char* args);
        bool HandleReloadSpellAffectCommand(char* args);
        bool HandleReloadSpellAreaCommand(char* args);
        bool HandleReloadSpellBonusesCommand(char* args);
        bool HandleReloadSpellChainCommand(char* args);
        bool HandleReloadSpellElixirCommand(char* args);
        bool HandleReloadSpellLearnSpellCommand(char* args);
        bool HandleReloadSpellProcEventCommand(char* args);
        bool HandleReloadSpellProcItemEnchantCommand(char* args);
        bool HandleReloadSpellScriptTargetCommand(char* args);
        bool HandleReloadSpellTargetPositionCommand(char* args);
        bool HandleReloadSpellThreatsCommand(char* args);
        bool HandleReloadSpellPetAurasCommand(char* args);
        bool HandleReloadDisablesCommand(char* args);

        bool HandleReloadSpellLinkedCommand(char* args);
        bool HandleResetAchievementsCommand(char* args);
        bool HandleResetAllCommand(char* args);
        bool HandleResetHonorCommand(char* args);
        bool HandleResetLevelCommand(char* args);
        bool HandleResetSpellsCommand(char* args);
        bool HandleResetStatsCommand(char* args);
        bool HandleResetTalentsCommand(char* args);

        bool HandleSendItemsCommand(char* args);
        bool HandleSendMailCommand(char* args);
        bool HandleSendMessageCommand(char* args);
        bool HandleSendMoneyCommand(char* args);

        bool HandleSendMassItemsCommand(char* args);
        bool HandleSendMassMailCommand(char* args);
        bool HandleSendMassMoneyCommand(char* args);

        bool HandleServerCorpsesCommand(char* args);
        bool HandleServerExitCommand(char* args);
        bool HandleServerIdleRestartCommand(char* args);
        bool HandleServerIdleShutDownCommand(char* args);
        bool HandleServerInfoCommand(char* args);
        bool HandleServerLogFilterCommand(char* args);
        bool HandleServerLogLevelCommand(char* args);
        bool HandleServerMotdCommand(char* args);
        bool HandleServerPLimitCommand(char* args);
        bool HandleServerResetAllRaidCommand(char* args);
        bool HandleServerRestartCommand(char* args);
        bool HandleServerSetMotdCommand(char* args);
        bool HandleServerShutDownCommand(char* args);
        bool HandleServerShutDownCancelCommand(char* args);

        bool HandleTeleCommand(char* args);
        bool HandleTeleAddCommand(char* args);
        bool HandleTeleDelCommand(char* args);
        bool HandleTeleGroupCommand(char* args);
        bool HandleTeleNameCommand(char* args);

        bool HandleTriggerActiveCommand(char* args);
        bool HandleTriggerNearCommand(char* args);
        bool HandleTriggerCommand(char* args);

        bool HandleUnBanAccountCommand(char* args);
        bool HandleUnBanCharacterCommand(char* args);
        bool HandleUnBanIPCommand(char* args);

        bool HandleWpAddCommand(char* args);
        bool HandleWpModifyCommand(char* args);
        bool HandleWpShowCommand(char* args);
        bool HandleWpExportCommand(char* args);

        bool HandleHelpCommand(char* args);
        bool HandleCommandsCommand(char* args);
        bool HandleStartCommand(char* args);
        bool HandleDismountCommand(char* args);
        bool HandleSaveCommand(char* args);

        bool HandleSummonCommand(char* args);
        bool HandleAppearCommand(char* args);
        bool HandleGroupgoCommand(char* args);
        bool HandleRecallCommand(char* args);
        bool HandleAnnounceCommand(char* args);
        bool HandleNotifyCommand(char* args);
        bool HandleGPSCommand(char* args);
        bool HandleTaxiCheatCommand(char* args);
        bool HandleWhispersCommand(char* args);
        bool HandleModifyDrunkCommand(char* args);
        bool HandleSetViewCommand(char* args);

        bool HandleLoadScriptsCommand(char* args);

        bool HandleGUIDCommand(char* args);
        bool HandleItemMoveCommand(char* args);
        bool HandleDeMorphCommand(char* args);
        bool HandlePInfoCommand(char* args);
        bool HandleMuteCommand(char* args);
        bool HandleUnmuteCommand(char* args);
        bool HandleMovegensCommand(char* args);

        bool HandleCooldownCommand(char* args);
        bool HandleUnLearnCommand(char* args);
        bool HandleGetDistanceCommand(char* args);
        bool HandleModifyStandStateCommand(char* args);
        bool HandleDieCommand(char* args);
        bool HandleDamageCommand(char* args);
        bool HandleReviveCommand(char* args);
        bool HandleModifyMorphCommand(char* args);
        bool HandleAuraCommand(char* args);
        bool HandleUnAuraCommand(char* args);
        bool HandleLinkGraveCommand(char* args);
        bool HandleNearGraveCommand(char* args);
        bool HandleExploreCheatCommand(char* args);
        bool HandleLevelUpCommand(char* args);
        bool HandleShowAreaCommand(char* args);
        bool HandleHideAreaCommand(char* args);
        bool HandleAddItemCommand(char* args);
        bool HandleAddItemSetCommand(char* args);

        bool HandleBankCommand(char* args);
        bool HandleChangeWeatherCommand(char* args);
        bool HandleKickPlayerCommand(char* args);

        bool HandleTicketAcceptCommand(char* args);
        bool HandleTicketCloseCommand(char* args);
        bool HandleTicketDeleteCommand(char* args);
        bool HandleTicketInfoCommand(char* args);
        bool HandleTicketListCommand(char* args);
        bool HandleTicketMeAcceptCommand(char* args);
        bool HandleTicketOnlineListCommand(char* args);
        bool HandleTicketRespondCommand(char* args);
        bool HandleTicketShowCommand(char* args);
        bool HandleTickerSurveyClose(char* args);

        bool HandleMaxSkillCommand(char* args);
        bool HandleSetSkillCommand(char* args);
        bool HandleRespawnCommand(char* args);
        bool HandleComeToMeCommand(char* args);
        bool HandleCombatStopCommand(char* args);
        bool HandleRepairitemsCommand(char* args);
        bool HandleStableCommand(char* args);
        bool HandleWaterwalkCommand(char* args);
        bool HandleQuitCommand(char* args);

        bool HandleMmapPathCommand(char* args);
        bool HandleMmapLocCommand(char* args);
        bool HandleMmapLoadedTilesCommand(char* args);
        bool HandleMmapStatsCommand(char* args);
        bool HandleMmap(char* args);
        bool HandleMmapTestArea(char* args);
        bool HandleMmapTestHeight(char* args);

#ifdef ENABLE_PLAYERBOTS
        bool HandlePlayerbotCommand(char* args);
        bool HandlePlayerbotConsoleCommand(char* args);
        bool HandleAhBotCommand(char* args);
#endif

        //! Development Commands
        bool HandleSaveAllCommand(char* args);

        Player*   getSelectedPlayer();
        Creature* getSelectedCreature();
        Unit*     getSelectedUnit();

        // extraction different type params from args string, all functions update (char** args) to first unparsed tail symbol at return
        void  SkipWhiteSpaces(char** args);
        bool  ExtractInt32(char** args, int32& val);
        bool  ExtractOptInt32(char** args, int32& val, int32 defVal);
        bool  ExtractUInt32Base(char** args, uint32& val, uint32 base);
        bool  ExtractUInt32(char** args, uint32& val) { return ExtractUInt32Base(args, val, 10); }
        bool  ExtractOptUInt32(char** args, uint32& val, uint32 defVal);
        bool  ExtractFloat(char** args, float& val);
        bool  ExtractOptFloat(char** args, float& val, float defVal);
        char* ExtractQuotedArg(char** args, bool asis = false);
        // string with " or [] or ' around
        char* ExtractLiteralArg(char** args, char const* lit = NULL);
        // literal string (until whitespace and not started from "['|), any or 'lit' if provided
        char* ExtractQuotedOrLiteralArg(char** args, bool asis = false);
        bool  ExtractOnOff(char** args, bool& value);
        char* ExtractLinkArg(char** args, char const* const* linkTypes = NULL, int* foundIdx = NULL, char** keyPair = NULL, char** somethingPair = NULL);
        // shift-link like arg (with aditional info if need)
        char* ExtractArg(char** args, bool asis = false);   // any name/number/quote/shift-link strings
        char* ExtractOptNotLastArg(char** args);            // extract name/number/quote/shift-link arg only if more data in args for parse

        char* ExtractKeyFromLink(char** text, char const* linkType, char** something1 = NULL);
        char* ExtractKeyFromLink(char** text, char const* const* linkTypes, int* found_idx = NULL, char** something1 = NULL);
        bool  ExtractUint32KeyFromLink(char** text, char const* linkType, uint32& value);

        uint32 ExtractAccountId(char** args, std::string* accountName = NULL, Player** targetIfNullArg = NULL);
        uint32 ExtractSpellIdFromLink(char** text);
        ObjectGuid ExtractGuidFromLink(char** text);
        GameTele const* ExtractGameTeleFromLink(char** text);
        bool   ExtractLocationFromLink(char** text, uint32& mapid, float& x, float& y, float& z);
        bool   ExtractRaceMask(char** text, uint32& raceMask, char const** maskName = NULL);
        std::string ExtractPlayerNameFromLink(char** text);
        bool ExtractPlayerTarget(char** args, Player** player, ObjectGuid* player_guid = NULL, std::string* player_name = NULL);
        // select by arg (name/link) or in-game selection online/offline player

        std::string playerLink(std::string const& name) const { return m_session ? "|cffffffff|Hplayer:" + name + "|h[" + name + "]|h|r" : name; }
        std::string GetNameLink(Player* chr) const;

        GameObject* GetGameObjectWithGuid(uint32 lowguid, uint32 entry);

        // Utility methods for commands
        bool ShowAccountListHelper(QueryResult* result, uint32* limit = NULL, bool title = true, bool error = true);
        void ShowFactionListHelper(FactionEntry const* factionEntry, LocaleConstant loc, FactionState const* repState = NULL, Player* target = NULL);
        void ShowItemListHelper(uint32 itemId, int loc_idx, Player* target = NULL);
        void ShowQuestListHelper(uint32 questId, int32 loc_idx, Player* target = NULL);
        bool ShowPlayerListHelper(QueryResult* result, uint32* limit = NULL, bool title = true, bool error = true);
        void ShowSpellListHelper(Player* target, SpellEntry const* spellInfo, LocaleConstant loc);
        void ShowPoolListHelper(uint16 pool_id);
        void ShowTicket(GMTicket const* ticket);
        void ShowTriggerListHelper(AreaTriggerEntry const* atEntry);
        void ShowTriggerTargetListHelper(uint32 id, AreaTrigger const* at, bool subpart = false);
        bool LookupPlayerSearchCommand(QueryResult* result, uint32* limit = NULL);
        bool HandleBanListHelper(QueryResult* result);
        bool HandleBanHelper(BanMode mode, char* args);
        bool HandleBanInfoHelper(uint32 accountid, char const* accountname);
        bool HandleUnBanHelper(BanMode mode, char* args);
        void HandleCharacterLevel(Player* player, ObjectGuid player_guid, uint32 oldlevel, uint32 newlevel);
        void HandleLearnSkillRecipesHelper(Player* player, uint32 skill_id);
        bool HandleGoHelper(Player* _player, uint32 mapid, float x, float y, float const zPtr = 0.0f, float const ortPtr = 0.0f);
        bool HandleGetValueHelper(Object* target, uint32 field, char* typeStr);
        bool HandlerDebugModValueHelper(Object* target, uint32 field, char* typeStr, char* valStr);
        bool HandleSetValueHelper(Object* target, uint32 field, char* typeStr, char* valStr);

        bool HandleSendItemsHelper(MailDraft& draft, char* args);
        bool HandleSendMailHelper(MailDraft& draft, char* args);
        bool HandleSendMoneyHelper(MailDraft& draft, char* args);

        template<typename T>
        void ShowNpcOrGoSpawnInformation(uint32 guid);
        template <typename T>
        std::string PrepareStringNpcOrGoSpawnInformation(uint32 guid);

        /**
         * Stores informations about a deleted character
         */
        struct DeletedInfo
        {
            uint32      lowguid;                            ///< the low GUID from the character
            std::string name;                               ///< the character name
            uint32      accountId;                          ///< the account id
            std::string accountName;                        ///< the account name
            time_t      deleteDate;                         ///< the date at which the character has been deleted
        };

        typedef std::list<DeletedInfo> DeletedInfoList;
        bool GetDeletedCharacterInfoList(DeletedInfoList& foundList, std::string searchString = "");
        std::string GenerateDeletedCharacterGUIDsWhereStr(DeletedInfoList::const_iterator& itr, DeletedInfoList::const_iterator const& itr_end);
        void HandleCharacterDeletedListHelper(DeletedInfoList const& foundList);
        void HandleCharacterDeletedRestoreHelper(DeletedInfo const& delInfo);

        void SetSentErrorMessage(bool val) { sentErrorMessage = val;};
    private:
        WorldSession* m_session;                            // != NULL for chat command call and NULL for CLI command

        // common global flag
        static bool load_command_table;
        bool sentErrorMessage;
};

class CliHandler : public ChatHandler
{
    public:
        typedef void Print(void*, char const*);
        explicit CliHandler(uint32 accountId, AccountTypes accessLevel, void* callbackArg, Print* zprint)
            : m_accountId(accountId), m_loginAccessLevel(accessLevel), m_callbackArg(callbackArg), m_print(zprint) {}

        // overwrite functions
        const char* GetMangosString(int32 entry) const override;
        uint32 GetAccountId() const override;
        AccountTypes GetAccessLevel() const override;
        bool isAvailable(ChatCommand const& cmd) const override;
        void SendSysMessage(const char* str) override;
        std::string GetNameLink() const override;
        bool needReportToTarget(Player* chr) const override;
        LocaleConstant GetSessionDbcLocale() const override;
        int GetSessionDbLocaleIndex() const override;

    private:
        uint32 m_accountId;
        AccountTypes m_loginAccessLevel;
        void* m_callbackArg;
        Print* m_print;
};

#endif
