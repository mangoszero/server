MaNGOS Zero Changelog
=====================
This change log references the relevant changes (bug and security fixes) done
in recent versions.

0.22 (2021-01-01 to now) - "Awaken the Spirits"
----------------------------------------------------------
* Initial Release 22 Commit


0.21 (2017-01-02 to 2021-01-01) - "The Battle for Azeroth"
----------------------------------------------------------
Many Thanks to all the groups and individuals who contributed to this release.
- 520+ Commits since the previous release.

Code Changes:
=============
* Removed the old SD2 scripts and Added the new unified SD3 Submodule
* Removed the individual extractor projects and added a unified Extractors Submodule

* .ticket command rework
* 50% player damage to mob required for reward Unchanged TC backport. Not fully tested.
* [Appveyor] Remove no-longer needed file
* [Build] Attempt to fix osx build
* [BUILD] Change default directory for install server and configs (#74)
* [Cleanup] Remove tabs which have crept into the source
* [DB] Update expected world DB version
* [DB] Update mangos to expect the base database versions
* [DB] Updated to expect Rel21_14_067_kodo_roundup_tidyup
* [DB] updated to latest db structure version
* [DBDocs] Remove DBDocs Editor as no longer required
* [DbDocs] The Big DB documentation update
* [DEP] Some missed date changes
* [Dep] Update dep submodule
* [DEP] Update Stormlib v9.21
* [DEPS] Update zlib version to 1.2.8
* [Docs] Fix some broken links
* [EasyBuild] Add support for newer MySQL/MariaDB versions
* [EasyBuild] EasyBuild updated to V2
* [EasyBuild] Fix cmake crash on French OS
* [EasyBuild] Fix some more French OS crashes
* [Easybuild] Fixed a crash. Thanks 
* [EasyBuild] ignore easybuild created debug files
* [EASYBUILD] Move the source of the downloads from external sites to internal
* [Easybuild] Reactivate VS2019 support
* [EasyBuild] Updated Easybuild to v1.8
* [EasyBuild] Updated MySQL and Cmake library locations
* [EasyBuild] Updated submodule
* [EasyBuild] Updated to remove some build options
* [EasyBuild] Updated to Support modified build system and enhancements
* [Eluna] Add conditionals around code
* [Eluna] Fix crash when accessing players not on any map
* [Eluna] Remove Eluna Submodule URL
* [ELUNA] SpellAttr fixes and more #120
* [Eluna] Update Eluna
* [Eluna] Update Eluna to latest version
* [Eluna] Updated Eluna submodule
* [Extractors] Fix file locations
* [EXTRACTORS] Improvement made to getBuildNumber()
* [Extractors] Minor cleanup to fix some warning messages
* [FIX] full block {} expected in the control structure. Part 1
* [FIX] full block {} expected in the control structure. Part 2
* [FIX] Revert 'Fix BagSize limit to 36' due to it breaking Auction and Character Item loading
* [Linux] Fix playerbots in getmangos.sh. Thanks Tom Peters
* [PlayerBot] Fixed a SQL typo
* [Realm] fix account table errors
* [Realm] prevent a regression bug on VS2017/32Bit for mangosZero
* [realm] updated realmd module
* [REALM] Updated submodule
* [Realmd] Fixed Broken Patching system
* [Realmd] Resolve SRP6a authentication bypass issue. Thanks 
* [SD3] Fix ashara compile error on Linux. Thanks H0zen
* [SD3] Fix error in submodule
* [SD3] Fix naxx crash on Linux. Thanks H0zen
* [SD3] Fix quest Kodo Roundup (#69)
* [SD3] Revert SD3 to earlier version (until all the issues are fixed
* [SD3] SpellAttr fixes and more #120
* [SD3] Step back SD3 until eluna is ready
* [SD3] Update Onyxia script
* [SD3] Update SD3 submodule
* [SD3] Updated ScriptDev3 submodule
* [SD3] Updated Scriptdev3 submodule
* [SD3] Updated SD3 submodule
* [Spell] Remove vanish buff is user cancel stealth
* [Spells] Anger Management should only work out of combat I can't figure out how to make the decay part work yet
* [Submodules] Updates dep and SD3
* [TOOLS] Fixed mmap extractor binary name used in MoveMapGen.sh
* [TOOLS] Fixed mmap extractor binary name used in various scripts
* A bit better decoupling of the movement speeds of pet and its master.
* Accounting disabled LoS check while mob casting
* Add "go" and "to_me" option to ".mmap path" command.
* Add .groupaura command (#105)
* Add a debug command to show all possible random point for selected creature.
* Add change back in
* Add check and error message to schedule_wakeup call. Thanks H0zen
* Add Codacy badge and link
* Add const descriptor to GetSingleCreatureFromStorage method.
* Add Core support for Franklin the Amiable / Klinfran the crazed (#118)
* Add core support for spell 29201 Basic version of Corrupted Mind spell for Loatheb encounter in Naxxramas (original)
* Add game event hooks and update eluna version
* Add IsSeatedState() to handle AURA_INTERRUPT_FLAG_NOT_SEATED.
* Add mangos string Language.h generator (#104)
* Add mangosd full versioning information on windows
* Add missing Eluna call
* Add necessary pthread flag for clang
* add new mangos 'family' icons. Thanks UnkleNuke for the original design
* Add positive exception for spells 13139,23182,23445 and 25040 Net-o-Matic special effect, Evil Twin, Mark of Nature and Mark of Frost are debuff and should not be removable by players
* Add protection for New Thread pool reactor settings from conf
* Add realmd full versioning information on windows
* Add some additional detail to some pool error messages. Thanks H0zen
* Add state for GM command completed quests. Thanks H0zen for assistance
* Add support for new comment column
* add support of ubuntu 18.04 in file getmangos.sh (#69)
* Add Ubuntu 19.04 case for Prerequisites install (#77)
* Added `disables` table
* Added and updated map mthread
* Added Dbdocs editor
* Added IsLockInRange check when dealing with opening locks with a spell
* Added IsSpellHaveEffect
* Added minor account table update for PlayerBotAI
* Added Missing function
* Added MySql 8.0 support, see notes
* Added support for openssl on OSX systems running OpenSSL 1.0.2g (installed using homebrew). (#115)
* Added unified extractor submodule
* Added: LazyMaNGOS Auto Installer (Debian)
* Adding DB log, minor refactoring
* Adding new distribution support (Fedora) (#16)
* Adding support for Player Bots submodule in installer. (#20)
* Adding support for Ubuntu: Curl dependencies added - Adding support when several WoW clients path are detected. Only the first one is selected - Adding support for database updates. Only last folder (alphabetically sorted) will be taken
* Adding wrapper to ScriptedAI for target selection
* Adds custom emote to wyrmthalak script
* Adds infrastructure to support talent based effects in the future.
* Adjust Revision number
* Adjust the source code and build enviornment so that Mangos Zero will build on ARM32. (#79)
* Adjusted Startup Revision
* Adjusted version info
* Allow creatures to handle several EVENT_T_RECEIVE_EMOTE instead of one.
* Allow dying creatures to deal damage when casting spells.
* Allow GAMEOBJECT_TYPE_BUTTON and TRAP respawn using SCRIPT_COMMAND_RESPAWN_GAMEOBJECT.
* Allow GAMEOBJECT_TYPE_GOOBER to start DBScripts on GO Use.
* Allow OpenSSL version up to 1.1.x
* Allow pet motion speed change like other mobs
* Allow SD3 scripted dummy and script spelleffects upon players
* Any triggered by aura spell should be casted instantly (a weaker condition).
* Any triggered spell should be casted instantly.
* Applied Extractor Updates
* Apply style fix
* Appveyor
* Appveyor supplied fix for openSSL 1.0
* Arena Master Quest Fix
* Attempt to fix Mutanus the Devourer event.
* AURA_STATE_HEALTHLESS_20_PERCENT apply only on alive targets
* Autobroadcast should be disabled by default
* AutoBroadcast system.
* Bestial Swiftness: should be incative while pet following.
* Better handling of movement generators when fear/confuse ends for a player.
* Boss Gar tidy up
* Build - GCC
* C++11 build fix
* Call Reset() after AI ctor; AT 522 script fix
* Case statement not terminated properly
* Change return value of a getting spell target method from Unit* to ObjectGuid
* Changed email return for item that can't be equiped anymore. Before the email was sent with an empty body and the subject was to long to be displayed in the player email. Now the Email is sent with the subject 'Item could not be loaded to inventory.' and the body as the subject message before. (#71)
* Changes made to bring in-line with One
* Channel world: disabling join/leave announces
* Character manaregen: looks like one /2 was excessive
* Charm effect on player causes control loss and motion interrupt
* Check pet power before it casts
* Checked enum ResponseCodes
* Chest with quest loot deactivation
* Clang return error fix
* Clarify some issues regarding negative angles. The client seems to handle them strange.
* Clarify some logic in SpellMgr.cpp
* Clarify weather packet
* Classic Mistletoe Spell Fix
* Cleaning of the new GO commands implementation
* cleanup inspired by GCC build
* Cleanup: no more used after SD2 reactoring WorldTemplate stuff
* CMSG_MOVE_SET_RAW_POSITION handling. ToDo: use also MSG_MOVE_SET_RAW_POSITION_ACK.
* Command: debug recv Places the packet described in the ropcode.txt server-side file into own packet queue (counterpart of debug send opcode).
* Commands for GO manipulations (gobj): anim, state, lootstate
* Complete redesign of waypoint system
* Continuation of commit 1f1735ad0c81a6740a6345c0b7ae1c5d195628c6
* Core/Collision: Models with flag MOD_M2
* Corections made to indentations in script
* Correct .EXE release number back to Rel21
* Correct Heartbeat resist after my #39efe8c
* Correct SD2 return values
* correct spawn rules and configurability for new ore system
* Correct spell damage taken on melee attacks.
* Correct sutble typo
* Correct Typo for default status
* Correct warden typos. Thanks foereaper
* Corrected values of constants for SMSG_RAID_GROUP_ONLY
* Corrected website URL
* Correction made to valid m0 build numbers
* Correction to FindPlatform
* Corrections to previous commit. (#22)
* Corrections to the build system.
* Create a docker container image and runing it with docker-compose (#164)
* creature dead unsummon pet. (#6)
* Creature sleeping script altered
* Crowd Control handling improvements by Ono(Warlockbugs)
* DB log: correct check ID
* DBScripts-Engine: Implement new commands
* Delete unused Language.h enums (#103)
* DetailsEmote for quests (SMSG packet structure)
* Dire Maul fixes and gameObject AI implementation (#100)
* Disable access to PvP barracks for too low ranks and other faction.
* Disable BG: another way to do
* Disable spawns: base checking entry, additionally.
* Disables: DISABLE_TYPE_ITEM_DROP=10. Also fixed memory leak
* Disables: item drop disable Quest loot cannot be disabled here.
* Don't interrupt certain channeled spells
* Druid Clearcasting: do not drop aura if spellmod was not used (for 0 spellcost)
* Eluna update version - Fix VS 2013 build
* Eluna: Remove travis script for getting last eluna
* Enable areatrigger teleports in battleground
* Enabled autopooling code and fixed quest 5203 as well as Troll Beserking and Stoneform Racial
* enhance readability and correct numbers
* ensure bins are marked as executable (#108)
* Ensure the resurrection when entering instantiable map
* Expected Base DB updated to Rel21_22_024
* Experimental movement lag fix
* Explicit conversion ObjectGuid to uint64 (gcc build fix)
* Extra attack: allow to accumulate only next swing extra atttacks
* Extra attack: instant or on the next swing
* Extractor updates
* Fake combat state is also a reason to prevent mob evade
* Few improvements for Win32
* Few more SMSGs: cleanup
* Few SMSG_CHANNEL chat opcodes (#31)
* Finkle Einhorn is now spawned after skinning The Beast in UBRS. Thanks NostraliaWow
* First major styling change
* Fix "Unknown player" bug Fixed old bug where players where all are named "Unknown" after some log in / log out
* Fix .go creature command (#115)
* Fix .goname command
* Fix : Warlock Soul Drain will not give a second Soul Shard if victim is under "Shadowburn" (#110)
* fix a bug in Player::addSpell causing wrong values in skills and talents.
* Fix a bug in previous commit where some wmo's were not extracted at all.
* Fix a client freeze caused by malformed paths
* Fix a crash. Using format specifiers without matching values is a no-go and clean up.
* Fix a small typo in previous commit.
* Fix ACE_TSS usage.
* Fix AHBot SetPricesOfItem (#87)
* Fix an issue where players needed rewardcount + 1 empty bag slots to accept quest rewards.
* fix and simplify the Player::addSpell function.
* Fix Arcane Missile self cast bug.
* Fix Archlinux build
* Fix bot PCH
* Fix bsd build after 756c8ff7
* Fix build error on Travis for windows builds
* Fix Cannibalize
* Fix channeling spells for unit (will not cast channeling spells when walking, attacking etc.)
* Fix compiler warning.
* Fix crash during gm .saveall
* Fix crash on taming rare creatures.
* Fix crash when loading invalid vmap data.
* Fix crash when using console commands
* Fix crash when using DB script command 34 (TERMINATE_CONDITION).
* Fix crash when using Eye of Kilrogg (spell id 126)
* Fix deeprun rat roundout crash
* Fix Deeprun spell used on player
* Fix default case for ahbot
* Fix Devouring Plague
* Fix displaying the right ranks of spells in spellbook
* Fix Divine Shield not absorbing lava or slime damage. thanks NostraliaWow
* fix doubling the text displayed in channels chat when playerbots building is enabled.
* Fix Eluna hook calls
* Fix Eluna OnCreatureKill
* Fix false error display - Bloodthirst triggers
* Fix Feral Swiftness talent
* Fix for branching within exclusiveGroup questline.
* fix for last stand 1hp bug
* fix for spell 24732 Bat Costume
* Fix FreeBSD build and clean spaces.
* Fix Gnomish Death Ray.
* Fix Gnomish Mind control cap
* Fix Gnomish Ray Shrink.
* Fix Ground and EarthBind Totems
* Fix Grounding Totem delay bug.
* Fix Improved Shield Block Talent Its additional charge was removed by a Dummy Effect.
* Fix incorrect enum in previous commit
* Fix incorrect usage of ACE_Guard
* Fix instance cleanup at startup (#99)
* Fix invalid table reference for playerbots
* Fix item Lifegiving Gem, in classic it was providing 15% health.
* Fix level up health text
* Fix Linux build
* Fix Linux include
* fix linux shell script error. (#82)
* Fix Mage's frost nova damage breaking aura
* Fix Mind Soothe aggro.
* fix missing include <string.h>
* fix Movement::MoveSpline::ComputePosition to return only positive angles.
* Fix mysql lib location for 5.0 and 5.1
* Fix non PCH build and update Eluna
* Fix NPC's running to the first waypoint
* Fix of the previous: copy mangosd config
* Fix OpenSSL travis for mac
* Fix Out Of Tree error for Linux builds
* Fix Paladin Hammer of Wrath,Judgement of Command,Seal of Command PROC,Seal of Righteousness Dummy Proc receive benefit from Spell Damage and Healing.
* Fix part of NPC localized text cannot be displayed.
* Fix pdump write command and add check to pdump load (#106)
* Fix player kicking
* Fix playerbot module building
* Fix potential NullPointerException on C'Thun (#107)
* Fix previous commit.
* Fix PvPstats table to fit with its web app
* Fix Q7363_Stave_of_the_ANcient_P1_SOlenor_the_slayer (#112)
* fix Quest 7603 - Kroshius
* Fix quest credit after erroneous #021aa8c
* Fix quest rewards appearing twice in chat
* Fix quests 4512 & 4513
* fix reference to dockerFiles to match with real files name (#92)
* Fix resource leaks in DBCFileLoader.
* Fix Ritual of Summoning in dungeons (#18)
* Fix seg fault on bots rolling
* Fix send mail and send item commands (#95)
* Fix server crash due to incorrect SQL parameter bindings.
* Fix server crash on CONDITION_GAMEOBJECT_IN_RANGE check.
* Fix server crash on shutdown
* Fix server crash. Thank H0zen/mpfans
* Fix Simone the seductress (#121)
* Fix sleeping peon in durotar
* Fix SMSG_WHO show players matching given criteria.
* Fix some codacy detected issues
* Fix some compiler warnings and project sync
* Fix some include paths in tools.
* Fix some minor typos
* Fix spell 19714 is buff instead of debuff. May fix also other spells that use same aura.
* Fix spelling of IsSwimmable.
* Fix spells with the target combination (TARGET_SCRIPT, TARGET_SELF).
* Fix stealthing animation for group members.
* Fix Talent improved Seal of Righteousness. 
* Fix TARGET_SELF-TARGET_SCRIPT target combination.
* Fix temporary enchantment duration
* Fix text typo
* Fix the AuctionHouseBot seller not posting auctions on some architectures/builds The issue was a forgotten initialization.
* Fix the description field for realm and characters database. Also update the revision.h
* Fix the network module.     - This fix must be applied on all cores.     - Solved a nasty race condition between network threads.
* Fix to 729bc32: resurrect only in case dungeon target map
* Fix tracking spells specific
* Fix Travis build on OS X.
* Fix typing error for High Priestess Jeklik
* fix typo
* Fix typo in VMap BIH generation.
* Fix uint32_t errors (VS 2013)
* Fix Unit::SetConfused to work on players.
* Fix warden crash for NYI builds
* Fix Warden disconnect bug
* Fix whisper blocking
* Fix wrong guid being sent in HandleQuestPushResult.
* Fix wrong include
* Fix wrong PartyResult enum value.
* Fix wrong use of uninitialized locks.
* FIX-CENTOS-BUILD Added epel repo
* FIX-CENTOS-BUILD Fixed centos 7 build
* fixed MacOS X build
* Fixed a bug where health would not regenerate unless your rage was above one (1) point, making warriors the elite master class.
* Fixed a bug where the pickpocket spell would share picked up money with the caster's group. Fixes #2.
* Fixed a bug where turning in a quest involving multiple rewards would sometimes not reward the items due to a faulty bag space check. Fixes #3.
* Fixed a compilation warning caused by a signed/unsigned mismatch.
* Fixed dead URL
* Fixed dos/unix file problem
* Fixed double fclose() in genrevision and general tidy up
* Fixed incorrect UpdateField.h value BANKBAG_SLOT_LAST (#68)
* Fixed instant 'Failed attempt' on gathering
* Fixed memory issue with msbuild build
* Fixed OpenSSL location (#118)
* Fixed output error in .mmap path
* Fixed PCH build issues
* Fixed PCH warnings and bot add command
* Fixed some annoyances
* Fixed submodules checkout.
* Fixed Tamed Kodo 5 automatically disappear
* Fixed wand damage immunity
* fixed wrong path spelling (#70)
* Fixes daze absorb and duplicate target flags (#67)
* Fixes Error "There is no game at this location" (#172)
* fixes error of uninitialized member of World
* Fixing a glitch.
* fixing beast lore Aura
* Fixing crash at player leave map during loot roll
* Fixing few warnings
* Forgot a couple required OSX and Unix library names
* g++ was not installed without build-essential (#80)
* Game/MiscHandler: max matchcount for SMSG_WHO is now configurable
* Garr - Improved firesworn on death exploding and added enrage ability
* Garr adds explosion fix
* Gcc build fix: no cbegin()/cend() container members
* Gcc build: get rid of few warnings
* GetRolls no longer needed, caused seg fault
* GM Commands files reorganisation (#96)
* Gm ticket handling fixes (#90)
* GM_tickets_handling_fixes_pt2 (CORE) (#93)
* Gossip Item Script support (#124)
* GroupHandler: prevent cheater self-invite
* Hai'shulud script update.
* Heartbeat resist first implementation
* HITINFO_BLOCK value, yet unused
* Holy Light and Seal of Righteousness PROC and Flash of Light receive benefit from Spell Damage and Healing too low.
* Honor flush and GetDateToday() method fix
* Implement .reset items command (#101)
* Implement .reset mail command + Increase max bags slots to 36 (#102)
* Implement Appveyor Windows testing
* Implement CMSG_MOVE_TIME_SKIPPED packet!
* Implement command localization (#97)
* Implement CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN
* Implement holes handling on map. Now GetHeight correctly returns no height when a hole is detected.
* Implement Inferno spell for Baron Geddon (classic version with TriggerSpell)
* Implement instance data call for OnCreatureDespawn.
* Implement movement wrappers for creatures and players
* Implement OpenSSL 1.1.x support
* Implement player rank in defensive channels
* Implement possibility to force enable/disable mmap-usage for specific creatures.
* Implement quest_relations table (#114)
* Implement SCRIPT_COMMAND_SEND_MAIL
* Implement SMSG_MOUNTRESULT, SMSG_DISMOUNTRESULT (yet unused)
* Implement the creature spell lists system. (#123)
* Implemented EasyBuild for windows
* Implemented school immunity for creature from database
* Implementing CAST_FAIL_NO_LOS EventAI
* Improve and Finalize new Weather system
* Improve confused, random and fleeing movement generators.
* Improve jerky player movement on high latency
* improved algorithm for finding a random reachable point on ground
* Improved Blink
* Improved Combat Movement handling
* Improved logic in ACTION_T_CAST
* Improved Rune game objects a lot
* Improved sap Talent
* Improved vmap-extractor.
* Improvements to the build system
* Improving Build system and removing Common.h clutter
* Include forgotten data file in vmap-extractor.
* Include location fix
* Incomplete text should not be shown for completed quests (#35)
* Instance Reset Fix
* Instant Flight Paths option added.
* Interrupt autoshot by deselecting target
* Introducing std::unordered_map/set and enforce C++11 on Linux
* Judgement of command not need CastCustomSpell
* Large rearrangement and cleanup of base cmake system
* LazyMaNGOS Re-Vamp
* Linux build fix
* linux/getmangos.sh: default to build client tools (#19)
* List aura - added number of aura stacks, see DB
* lity to force a level in Creature::SelectLevel()
* Local variable reassigned prior to reading
* Low-level spells cast by high-level players will receive smaller bonuses from +healing and +spell damage.
* Mage Arcane Power update
* Major refactoring of SD2 script system
* Make ACE build on other OSes too
* Make AppVeyor happy
* Make Deeprum tram active on server start and remain active
* Make GM max speed customisable through mangosd.conf (#89)
* Make Mangos compatible with newer MySQL pt2. Based by work by 
* Make Mangos Zero compatible with newer MySQL (#83)
* Make some errors more verbose.
* Make the .ban command remove characters from the world (instantly) Previously the .ban command would only disconnect players. Their character was staying ingame for multiple minutes.
* Make use of attribute SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY.
* Mark TypeList.h for deletion
* Massive improvements on Garr encounter
* Match client limits in cgaracter DB.
* Merged Vmaps extractor/assembler, updated scripts
* Minor cleanup Too lazy to find out what exactly was influenced by it, but it was wrong :) no effect != negative effect
* Minor corrections to the build system.
* Minor fixes for clang builds.
* Minor formatting cleanup
* Minor refactoring Like HasSpellEffect method here, other now global-scoped methods from SpellMgr.h.
* Minor refactoring of auto-replacing player spell ranks
* Minor refactoring of detect visibility code The predefined method of getting aura modifier is used for clarity.
* Minor syntax correction
* Misplaced SendGameObjectCustomAnim(): may be used for GOs only
* Missed delimiter (#91)
* Missed one spot in previous commit.
* Missing delimiter (#92)
* Modified Pet Owner combat state
* Molton Core cleanup
* More fixes. This solves the annoying issue of Basic Campfires not giving spirit buffs.
* More lock fixes. Also fix the .character level command
* More minor fixes for worldserver startup/shutdown on *nix
* More robust checks on mutex acquire.
* More SMSG cleanup
* More SMSG structure cleaned
* More SMSGs Minor structure corrections, some constants, comments, syntax
* More SQL delimiting for modern servers (#166)
* More thread-safety checks.
* Move core definition into cmake
* Move DB revision struct to cpp
* Move the license file
* Moved condition related to mage casting on self
* Movement changes
* MSG_QUERY_NEXT_MAIL_TIME fixing my fault
* MSG_QUEST_PUSH_RESULT packet structure fix
* MSG_RAID_READY_CHECK: fix raid ready check mechanic TODO: test limiting responce packets with the leader
* Multiple fixes (#116)
* My guess to 5875 client methods
* MySQL CMake Macro rewrite
* Naxx: Gothik - redesign
* Necessary include path for osx
* New build system
* New RNG engine for MaNGOS
* New thread pool reactor implementation and refactoring world daemon. (#8)
* non initialised variable.
* Normalize Username so that Telnet users can use lower case usernames
* Not so bright warning if DB content newer than core awaits
* Now we can inspect player when GM mode is ON (#98)
* ObjectAccessor rewrite
* Override spell range for script target spells when not provided.
* Paladin Reckoning bomb with the limitation.
* Paladin reckoning bomb.
* Partial fix for .gm fly command (#88)
* Partial revert of AHBot module
* Partial revert of PCH changes
* Partially debugged logging code.
* Player movement interrupt when fear/confuse fades Should be improved due to difference between StopMoving() and tMoving().
* Player::TeleportTo() signature change to request undelayed teleport.
* PLAYER_EVENT_ON_LOOT_ITEM fix for eluna. Thanks mostlikey
* Playerbot: crashfix
* Pool-System: Allow pooling non-lootable gameobjects
* Prevent memory corruption at DBC loading. Thanks to Lynx3d for investigating that issue
* Prevent Racial leaders PvP dropping due to faction rules
* Prevent Seal of Command damage overflow.
* Project Cleanup pt3
* Project tidy up and sync
* Project tidy up and sync pt2
* Q7636_P1_FIX_Solenor_the_slayer
* Racial Leaders (SMSG packet structure)
* reactivate mac os testing
* Reapply Properly display creatures with waypoint when they enter player range
* Reapply Properly display creatures with waypoint when they enter player range
* Refactor the areatrigger_teleport to use condition system.
* Refactoring db_scripts
* Refactoring Mount/Unmount: code split between Unit and Player
* Refactoring of #14
* Referencing SD3 update for fix codacy warnings (#122)
* Regex requires gcc 4.9 or higher
* Reload command do not announce to player accs
* Remove duplicate line in vmap-extractor.
* Remove few unused fields from SMSG packets. Other changes are either cosmetic or in comments form.
* Remove last reminents of obsolete npc_gossip table
* Remove obsolete code
* Remove policy CMP0005
* Remove Remnants of Two obsolete tables: npc_trainer_template & npc_vendor_template
* Remove unused include directives
* Removed character updates import and added config rename along with two required compilers
* Removed deprecated SD2
* Removed non-classic items from ah selection.
* Removed OpenSSL1.1.x blocker
* Removed SD2 database binding
* Removed SFN wildcards
* Removed the ancient ahbot module, if PLAYERBOTS enabled
* Removed the individual extractor projects and added a unified Extractors Submodule
* Removed the old SD2 scripts and Added the new unified SD3 Submodule
* Removed unnecessary cmake macro
* Removed unused methods.
* Rename the cmake variable respecting conventions
* ReputationMgr: set and clear AtWar flag appropriately
* Resisted pickpocket attempts now send the appropriate resist message. Closes #4
* Restore commit 'Mr Smite Weapon Correction'
* Restore previous commits. Includes rage decay changes.
* Rewrite TypeContainers using variadic templates
* Roge Stealth update
* Rogue - Improved Sap.
* Rogue Stealth corrections
* SD2: correct handling OnGossipSelect
* SD3 fix Artorius the doombringer (#117)
* SD3 fix linux compile and reference latest SD3 commit
* Seal of the Crusader deals less damage with each attack.
* Server Banner and Status redone
* Server-owned world channel
* Several fixes (#10)
* Several major improvements to Linux installer. (#15)
* Shared Final
* Skip item SPELLTRIGGER_ON_USE at form change.
* SMSG_BATTLEFIELD_LIST fix
* SMSG_GAMEOBJECT_QUERY_RESPONSE: better packer structure description
* SMSG_GAMEOBJECT_RESET_STATE, yet unused
* SMSG_GUILD_COMMAND_RESULT check of some constants
* SMSG_GUILD_QUERY_RESPONSE  better length estimation
* SMSG_ITEM_QUERY_SINGLE_RESPONSE a small syntactical elaboration
* SMSG_LOOT_RESPONSE cleanup Loot item numbering in the packet must be strictly sequential, since the number is used for memory offset calculation. Duplicate numbers will cause item overwrite, so it will not be shown in the loot by client.
* SMSG_NAME_QUERY_RESPONSE minor improvements
* SMSG_OPEN_CONTAINER implemented
* SMSG_PARTY_MEMBER_STATS_FULL fix
* SMSG_PET_CAST_FAILED: packet structure fix
* SMSG_PET_NAME_INVALID contains no data
* SMSG_QUESTGIVER_QUEST_FAILED elaboration
* SMSG_QUESTUPDATE_FAILED should be used to fail quest at client
* SMSG_READ_ITEM_FAILED a bit detailed
* SMSG_SET_FACTION_ATWAR: implement and use
* SMSG_SPELL_FAILURE: a more correct usage
* SMSG_SPELLLOGEXECUTE fix and some elaboration
* SMSG_TRADE_STATUS: minor cleanup of TradeStatus values
* SMSG_TRANSFER_ABORTED has no data except uint8 TransferAbortReason
* Some Minor Cleanup
* Some minor styling updates
* some missed dates
* Some pet fixes (#119)
* Spell attribute allowing heartbeart resist mechanic
* Spell class - Fix for inaccessible class data
* Spell_Miss will Restore Rage and Energy
* SpellAttr fixes and more #120
* Split the look-up portion of FindEquipSlot into a 2nd function ViableEquipSlots.
* Start dbscripts_on_spell for SPELL_EFFECT_TRIGGER_MISSILE with missing spell id.
* Stop casting non-instant spell when target is lost.
* Stop motion of players when fear/confuse fades
* Stranglethorn Vale sleeping creatures script
* Style cleanup from the Mangos Futures Team
* Style cleanup from the Mangos Futures Team
* Support C++11 and update Eluna
* Support of several client builds
* Suppress some clang warnings and cleanup redundant directives.
* Swapped 'dbscripts_on_creature_movement' warning with 'dbscripts' â€¦ (#97)
* Sync core function/variable names to match other cores: BG/BGArenas
* Tab cleanup
* The Endless Hunger script update
* The energy gained from "Thistle Tea" now decreases with levels past 40.
* The next Bestial Swiftness improvement Looks like the only issue left: the pet returns to the master at normal "follow" speed after combat.
* Trap stealth implementation
* Triage quest: fixing memory leak
* Trimming Ubuntu dependencies (#17)
* Typo and  Take Power.
* Update conf file to work with modern SQL servers
* update deeprun rat roundup script
* Update dependencies and necessary changes to tools
* Update deprecated ROW_FORMAT
* Update Mob Grey Level. Thanks TehPhoenixz
* Updated ACE to latest version and fixed appveyor
* Updated base revision to match base database
* Updated Easybuild to include VS2015 support
* Updated Eluna and SD3 submodules
* Updated Eluna and SD3 submodules
* Updated Expected content level
* Updated extractor submodule
* Updated extractors to fix movement bug. Will need to reextract
* Updated Realmd to now accept both local and external connections
* Updated SD3 submodule
* Updated Submodules: Eluna and Realmd
* Updated Unified Extractor subModule
* UpdateMask class refactoring
* Updating Debian Sources (#169)
* Upgrading checks for Database::CheckDatabaseVersion (#86)
* use canonical target names for zlib and bzip2
* Use SMSG_GMTICKET_UPDATETEXT; introduce named constants GMTICKET_RESPONCE_
* Using /who in a BG should only list players in the same instance.
* Validate the spawn distance passed to RandomMovementGenerator constructor.
* Various db fixes from external sources
* Warden checks refactoring
* Warden table redone to allow grouping of similar checks
* warden_refactor - db update
* Warrior - Sweeping Strikes, do not drop aura when stance changes.
* Warrior Talent - IMPROVED SHIELD BLOCK
* Wrong field order inside SMSG_LOGOUT_RESPONSE

DB Changes:
===========
* [DB] Update base db to 21_18_001
* [French] Updated Translations
* [Locale] Fix 'replace_BaseEnglish_with_xxx' file
* [Locale] Fix up installation script
* [Realm] fix account table errors
* [Realm] fix missing comma
* [Russian] Added some new translations
* Add .auragroup command db update (#102)
* Add DB support for Franklin the Amiable / Klinfran the crazed (#109)
* Add mangos_string needed for .reset items command
* Add missing lines in file
* Add missing spawns of NPC 12125
* Add missing table to backup scripts
* Added missing item 13325
* Added note to Rel21_23_001 in case of mysql timeout
* Anotehr Query update
* Atiesh, Greatstaff of the Guardian.
* Brainwashed Noble 596
* carriage returns - quest
* Dangerous! - object
* Database cleanup, based on findings from Magnet
* DB script for command localization (#93)
* DB update for Gm tickets handling fixes (#90)
* Fix .go creature command (#107)
* Fix 21.22.001 broken SQL update (#103)
* Fix > 100% loot chances for some groups Updated last sql file. (#96)
* Fix a number of errors.
* Fix for the quest 7636 (#110)
* Fix item_loot_template errors (#94)
* Fix Laughing Sisters models
* Fix NPC Artorius the Doombringer / Artorius the Amiable (#108)
* Fix position of a Menethil Sentry.
* Fix quest 3861 - CLUCK ! (#105)
* Fix target for  Empty cursed jar, Empty tainted jar and Empty pure sample jar.
* Fix texts for quest 6461
* Fix upper case in OfferRewardText for quest 8288
* Fixed for Dire Maul pt2 (#97)
* Fixed item 8632
* Fixed model size of NPCs 4046 and 11117
* Fixed text for item 10022
* Fixed Troll rogue starting spell
* GM_tickets_handling_fixes_pt2 (#92)
* Implement creature spells table. (#111)
* Instance Dire Maul Fixes (#95)
* Laris Geardawdl complete rework.
* Mangos strings for .reset mail command (#99)
* Many DB fixes (#101)
* Minor Updates.
* More DB cleanup based on Magnet findings
* More DB quest cleanup based on Magnet findings
* Prepare mangos strings for auto generating core enums (#100)
* Q7636_P1_FIX_Solenor_the_slayer (#106)
* Remove Blood Elf from AllowableRace field.
* Remove Dranei from AllowableRace field.
* Remove obsolete file
* Renamed incorrectly named file
* Set Searing Whelp to non-elite rank.
* Some minor corrections
* Structure version fixed
* Target type for Empty pure sample jar - Part 2.
* The big Command help syncup
* Tidy up loadDB file
* UBRS update part.1 (#91)
* Update 1 for mysql 8
* Update 2 for mysql 8
* Update deprecated ROW_FORMAT
* Updated commands help texts linked to core update about GM.MaxSpeedFactor
* Updated script to apply update from Rel21 folder (#98)
* Updates to InstallDatabases.sh (#112)
* WIP: Setup database with docker (#166)

0.20 (2015-03-23) - "New Beginnings"
------------------------------------
Many Thanks to all the groups and individuals who contributed to this release.
- 496 Commits since the previous release (upto aeb934ed)

* Some of the dependant file groups have been made into submodules
* i.e. all the dependant libraries (dep folder) and realmd

* 2 new quest related functions added
* acelite integration
* Add check for session being NULL so that we don't crash from client disconnect
* Add missing trap id for SendGameObjectCustomAnim call [c2494]   
* Add new Regen Health / Power flags and rename database fields
* Add script for areatrigger 4052 - AQ
* Add the ability to specify in the configuration file the cost of mounts
* Added a new DB helper script   
* Added base Battleground support for Eluna
* Added copy of Realmd.conf.in to output folder. Thanks to Madmax for pointing
* Added copyfiles step for VS2010/12/13
* Added eluna/sd2 flags to shared project
* Added GNU license to Linux build script
* Added missed commit
* Added requirement for OnQuestReward hook
* Added The_Spider_God
* Added tools icon to extraction tools
* Added VS2012 and 2010 solutions
* Adding support of GM ON/GM OFF Spells
* Adding support of one-hand/two-hand custom spell bonus coeff in the core.
* Adjusted a constructor to eliminate several warnings
* Adjusted MapId, MapX and MapY variables to increase performance
* Allow casting a random selected spell (c2594)
* Allow creating Non-Instance Maps without player (c2574) 
* Allow sending custom eventAI events to all units in range [c2491]    
* Allow target 60 to use script target whenever required (c2551)
* Applied AHBot updates from One
* Applied prepared solution directive to projects
* Apply a workaround to disable ClassLevelStats during DB switchover to this system. (c2528)
* AQ40 Skeram Image FIX
* Avatar of Hakkar event cleanup
* Avoid casting waterwalk on mounted or shapeshifted player. (c2585)
* Better value to check distance between owner and pet.
* Blacksmithing and Leatherworking specialisation scripts removed
* Bug Fix: Movement Visual Error
* Bug: Mighty Rage Potion and Elixir of giants
* Change Win32 behavior at abort() not blocking restart
* Check that policy exists before setting to old   
* Cleaned up Quest code slightly
* Cleanup and rework power type setting [z2500]    
* Code style tidy up
* corpse decay multiplier set to 0.5 as default
* Correct format of logging statement
* Corrected Cast for trainer levels. Thanks to Rochet for pointing
* corrected exe names in bat file
* Correctly log whispers
* Crashing typo in to .npc unfollow command
* Create 20005_windcaller_yessendra_reward_fix.sql
* Created Linux directory and added Linux build script
* Critical chances and dodge chances are incorrect for low levels. Thanks to Salja.
* Cuergo's Gold quest fix
* Documenting and Optimizing DBCStores related functions.
* Don't recalculate path when speed is changed
* Duel Range is too small - Minor update    
* Enable Shield Slam warrior spell
* EVENT_T_REACHED_WAYPOINT correction and improvement
* EventAI Codestyle Update (c2565)
* Felwood Corrupted Plants SD2 scripts
* Fix 'City' Instance Label bug
* fix .npc factionid command after creature_template change
* Fix a few more static code analysis warnings (c2593)
* Fix Ace / Recast build issues
* Fix ace import
* Fix auctionhouse crash on winning bids on time expire
* Fix BIH::intersectRay crash. Thanks TC
* Fix bug that cause AreaAura reaply because the code doesn't search the correct rank of it. (c2570)
* Fix build on Win with spaces in pathname
* Fix by Arkadus - Polymorph fix
* Fix cmake INSTALL
* Fix cmake macros for FreeBSD systems. Note: Since realmd is a submodule, it will need to be changed as well to fix the build completely
* Fix crash when database version doesn't match.    
* Fix creature change z position when speed change. [c2486]    
* Fix creature not stopping in some conditions. [z2485]    
* Fix Double to Float conversion warnings
* Fix druid mana on shapeshift
* Fix EVENT_T_OOC_LOS after [z2566] (z2587) Part of the code was missing in the backport, preventing the new code to be ever run.
* Fix for equipable item check
* Fix for memory spikes on pet despawn
* Fix for the Cuergo's Gold script fix
* Fix installation process and paths for windows
* Fix issue with afk playerin duel logout request from client after 30 mins.
* Fix Linux build. Thanks zackbcom
* Fix Linux CMake Lua Dependencies
* Fix linux tools build
* Fix logic bug in SelectAuraRankForLevel (c2569)
* Fix Mana Tide Totem. Closes #415
* Fix Master Loot + Group Loot Issues - Part 2
* Fix Mighty Rage Potion and Elixir of giants    
* Fix missing eluna/sd2 flags in mangosd. Thanks to foereaper for pointing
* Fix movement related ByteBuffer errors
* Fix Movement Visual Error    
* fix new mineral spawn system
* Fix Node Overlap error
* Fix one warning and suppress a few when using cmake 3.0   
* Fix pch
* Fix problem with scaling vmap model. VMap and MMap DOES NOT need to be rebuilded. (c2518)
* Fix raid instance reset crash and add a server command to force reset. Also added a new server command to force reset by an admin. (z2538)
* Fix rooted player continue their movement while rooted for other clients. (c2563)
* Fix scaling when shapeshift is used while any scale aura is applied. (c2584)
* Fix script for quests 1222 and 1270
* Fix scripts inclusion in win build after recent changes. 
* Fix skinning loot window bug. (c2573) Thanks to @TheTrueAnimal for pointing.
* Fix soap being an actual option, show status of engines as well   
* Fix some combat details for Thekal
* Fix some more reserved identifiers   
* Fix some static code analysis warnings and improve performance (c2554)
* Fix some warnings reported by static analysis (c2546)
* Fix Soul Shard while grouped
* Fix spell 9712
* Fix SpellFocus startup errors
* Fix startup and shutdown
* Fix static analysis const reference performance warnings
* Fix static analysis repeated expression performance warning (c2592)
* Fix static analysis warnings in AHBot (c2550)
* Fix stealthing animation for group members.
* Fix SYSCONFDIR as it was removed from the defines passed to the compiler
* Fix systemconfig.h errors in Prepared solutions
* Fix two build warnings
* Fix typo in script
* Fix up broken backport   
* Fix up movemap name in cmake
* fix various creature despawn problems
* Fix Warning on double to float constant conversion
* Fix warning reported by travis (c2547)
* Fixed all SD2 Level4 warnings
* Fixed correct install source for Extensions
* Fixed external Ace linking
* Fixed Gordunni Trap script
* Fixed internal Ace build
* Fixed Linux install path. 
* Fixed Linux shell scripts not running binaries
* Fixed missing server hooks
* Fixed possible mount abuse after leaving BattleGround (c2542)
* Fixed wrong order in SpellBonusEntry
* Fixes to #includes that used relative paths for Detour.
* Fixing bug with Warrior Execute
* Fixing channel issue where built-in channels weren't being detected correctly.
* Fixing Chest Loot Issue
* Fixing Creature Type issue
* Fixing Group Loot bugs
* Fixing group loot issue + chest loot issues
* Fixing Master Loot for already assigned item to players who had full bags.
* Fixing quest items impossible to loot when loot mode is master loot
* fixing skinning loot window can not be reopened
* Grammar correction
* grouploot chest fix
* Guardian of Blizzard no combat fix
* Hotfix - SpellBonus are applied twice
* Hotfix for Compilation error on range based loop.
* Icons overload general review and code cleaning (refactoring)
* Implement ACTION_T_CHANGE_MOVEMENT for EventAI
* Implement Battleground scores storage system (c2559)
* Implement condition CREATURE_IN_RANGE [c2492]    
* Implement CREATURE_FLAG_EXTRA_ACTIVE (c2578)
* Implement CreatureLinking Flag DESPAWN_ON_DESPAWN
* Implement EVENT_T_ENERGY for EventAI (c2588)
* Implement generic power handling for Creatures and Pets [c2488]   
* Implement Meetingstone support
* Implement removal of spells that don't have appropriate Spell AuraInterruptFlags using proc system (c2552)
* Implement script support for quest 2845
* Implement script support for quest 4261
* Implement script support for quests 5944 and 5862 (ending scene)
* Implement SCRIPT_COMMAND_SEND_AI_EVENT_AROUND for dbscripts engine
* Implement SMSG_INVALIDATE_PLAYER [c2483]    
* Implement spell effect 19395. This will fix quest 2987. 
* Implement TARGET_RANDOM_UNIT_CHAIN_IN_AREA (c2548) Also unify the TARGET_RANDOM_CHAIN_IN_AREA code
* Implement two more TEMPFACTION_TOGGLE flags (c2543) * UNIT_FLAG_PACIFIED * UNIT_FLAG_NOT_SELECTABLE
* Improve EventAI code engine a little bit to be more effective with EVENT_T_RANGE (c2564)
* Incorrect Entry ID for game object
* Initial cmake for windows work
* Initialize power type and power type values for creatures [c2489]    
* Learning Master Sword/Axe/Hammer skill spell corrections
* Loot Handler fix
* Major update of spell_bonus_data behavior
* Make Eluna optional when compiling   
* Make SD2 optional and fix a bug in config.h.cmake   
* Make tools build by default
* Make Travis happy
* Master loot profession fix
* Merge & fix pch build
* Merged in new build system
* Missing script registrations added back
* Modified Maps default precision from int to float, Also modifed warning Level from 3 to 4
* Move LFGMgr to correct location
* Moved enum to correct position in list
* Mr Smite Weapon Correction
* New event EVENT_T_REACHED_WAYPOINT
* New mineral spawn system
* New script condition added
* Patch to fix auras killing pets from causing a core dump
* PDB and output folders now in the same place
* Pets are now cancel attack on stay
* Pets are stopping now in correct position
* Player.cpp Item: Termination of loop, when no more stats to be added
* Quiver not being equipable in a bag slot fix
* Range is too small - Minor update
* Reapply [c2485] with correction for classic. 
* Rebuilt BuildTools solutions
* Rebuilt remaining tools solutions
* Removal of internal Realmd and added submodule   
* Remove build type from install path - doesnt work on linux
* Remove invisibility aura (aura 18) based on attribute (c2553) Passive and negative invisibility auras are not removed on entering combat
* Remove MANGOS_DLL_DECL and its use
* Remove support for Shiv
* Remove unused MAX_LEVEL_CLASSIC (c2531)
* Remove water walk when shapeshifting. (c2586)
* Rename fields after recent DB changes (c2597)
* Replaced hard coded values
* Restore power regen for pets
* Rewrite rest management (c2568) Fix Rest when logged in not working in some case. Also fix rest bonus according to wowwiki.
* Set DB Strict filter for creature_movement with MovementType <>2 (c2541)
* Set DT_POLYREF64 for linux platforms and configurations
* Should fix crash on transport to the same map
* Simple fix for Stealth is removed on fall damage problem 
* Small code optimizations. Thank to @tzurbaev (c2539)
* Some changes to Random chance calculation (c2567)
* some Creature Linking corrections and refactoring to Improve Startup efficency.
* Some fixes for compiling the new things on linux
* Spawning of The Unforgiven (Stratholme) scripted
* Speed Up EventAI OOC-LoS Event handling (c2566).
* Start of acelite integration
* Strat - The Unforgiven script code correction
* Temporarily fix crash on shutdown
* Totally Cueergo Gold's Script updated
* Triage quest now doable but still nerve-racking
* Tuken'kash (RFD) scripted
* Unified method of Shield Slam spell recongizing
* Update realm daemon so that it only presents clients with realms that it is compatible with.
* Updated build defs. Thanks to Rochet for pointing
* Updated recast references
* Visual fix for players trying to open chest/vein/plants simultaneously.
* Workaround for the wow-error client raised once a player on transport is leaving a map.
  

0.19 (2014-07-09) - Ludas Legacy
--------------------------------
Major changes for this build which require your attention when upgrading include:

Many Thanks to all the groups and individuals who contributed to this release.

* The *mangos-zero* build system has been overhauled again since the previous
  version was unwieldy for a majority of windows users and some devs !.
* In the win folder there is a new solution "BuildEverything" which does just that.
  It builds the Core, Extraction Tools and Scripts library.
* From this release Eluna scripting has been added. Many thanks to the Eluna Team

* Add a configurable delay between when a creature respawns 
* Add clang to the compiler list.
* Add script support for quest 660. 
* Added a few basics for embedding Lua, and updated docs accordingly.
* Added creature_equip_template sql 
* Added exception to allow/restrict use of specific mounts.
* Added files for Lua engine.
* Added missing definition 
* Added support for quest 2118. 
* Added Trash spell for Ebonroc, Firemaw and Flamegor
* Added VS2013 support
* All dispel spells will check if there is something to dispell
* Anger control and Superposition of combustion
* Backport recastnavigation
* Big rename of creature_template fields. 
* Change to random movement 
* Chat commands disabled for normal players. 
* Chat system refactoring All chat function now call same method
* Cleaned up references slightly 
* Cleanup world state sending 
* Closed a few resource leaks.
* Continued fixing EventAI documentation. 
* Correct Holy Light Cast time. 
* Correct some BG chat message missing target name. 
* Corrected some missed year updates 
* Correctly check aura on target for HealingBonusTaken instead. 
* Don't do periodic heal ticks if the target is already at max 
* Drop gossip for learning/unlearning advanced tailor skills 
* EventAI documentation style fixes. 
* Extract vmaps using correct path. 
* Finished Movemap-generator and added new BuildTools projects 
* Fix a bug that area was not correctly returned in some cases.
* Fix conflict between quest 8447 and 8733 
* Fix creature flee dont loose target. 
* Fix creature loose target while rooted. 
* Fix failed assertions while playing with the Mac client. 
* Fix fleeing creature walking instead of running. 
* Fix freeze if opcode.txt file does not exist on .debug send 
* Fix LANG_ADDON use on Guild Channels 
* Fix libmpq 
* Fix mistake in Twin Emperors script 
* Fix npc_Ame01 aggro texts 
* Fix PostgreSQL bindings and add support to build 
* Fix reloading horde controlled capture point 
* Fix Rockbiter Weapon and remove some TBC-specific code. 
* Fix some spellmods. 
* Fix spell 6346 (Fear Ward) 
* Fix spirit based mana regen  
* Fix Tauren druid size when shapeshifted. 
* Fix vmapexporter building on Mac OS X 
* Fixed a race condition in Thread. 
* Fixed an error in map extractor help message. 
* Fixed architecture name. 
* Fixed issue with PCH not working on some versions of GCC 
* Fixed the `debug send chatmmessage`.
* Get rid of bounding radius in GetNearPoint[2D] and ObjectPosSelector.
* Hide known recipes when the Usable box is checked in the AH. 
* Hunter pet's will now properly follow stay commands.
* Hunter pets will gain full experience while in group, instead of half.
* Implement 2 chat channel responses
* Implement basic follow-quest 2904 - A fine mess. 
* Implement escort quests 1222, 1270. 
* Implement new stats system for Health and Mana. 
* Implement script support for quest 3367. 
* Implemented fix from for issue #152 (fixed by user EdwardTuring) 
* Improve 'NPCs gets stuck in melee animation while casting'.
* Improve chat channel code.
* Improve handling of TargetMMGen.
* Improve random movement by permit randomly no wait time.
* Instantiate Lua engine as singleton.
* Integrate map extractor into build process.
* Let git_id create mysqldump alike dumps. 
* Loading/reloading classlevelstats. 
* Looting in groups has been corrected, and you should now be able to use group loot
* Looting will be cancelled upon movement.
* Makes Paladin's talent Benediction reduces mana cost
* Map extractor messages have been cleaned up.
* Map/Vmap extractor dependancies cleaned up 
* Maximum path length can be provided from random movement generator. 
* Merged ACTION_T_EMOTE_TARGET from mangos-three. 
* Mobs fleeing in fear will now properly flee, instead of running. 
* Move Lua engine to different folder.
* Movement flags for character movement have been updated for vanilla WoW.
* Movement map generator now will print clean usage instructions.
* Movement maps generator will report map version on startup.
* Pet stats are not modified by owner stats in vanilla.
* Power Infusion spell from priests now has more restrictions
* Prevent resource leaks in DBC loader.
* Properly detect clang and TR1 features.
* Readd clang to condition. 
* Remove a compile warning and better test with float. 
* Remove prebuild extractors 
* Remove unused argument in CalculateMeleeDamage. 
* Remove unused constructor overloads. 
* Removed off-mesh coordinates for maps not available in vanilla WoW.
* Removed redundant log output, and corrected a few compile time warnings.
* Removed script library entries for creatures not available in vanilla
* Rename m_respawnAggroDelay to make it more generic 
* Renamed DBC field format enums.  
* Reorder and unify the config's stat modifier handling. 
* Reorganize code to allow rage rewarded for critter type.  
* Reputation discount fix. Now there is a flat 10% discount 
* Reserve more DB_SCRIPT_STRING_ID. It seems that could be useful
* Restore warnings, and provide macros for C++11. 
* Script for quest 4021 Counterattack 
* Script Library Fixup #1 
* Script support for quest 2987 Gordunni cobalt 
* Set max gossip menu items to 32. 
* Simple fix to avoid corpse moving in some situation. 
* startup check for action_t_summon_unique 
* Summon ID checks now for EVENT_T_SUMMON_UNIQUE 
* support for creature_item_template 
* There is no unit flag for mounted state. 
* Prevent creatures from pulling too many nearby mobs
* Trade channel and guild recruitment channel still had notifications
* Trap GO now dispawn with their parrent object and special case for quest 
* Update all stats after creature respawn. 
* Update and improve script for quest 1249 
* Update Vmap asembler and others 
* Updated database structure and default contents. 
* Updated HandleMoveTimeSkippedOpcode to use proper definition
* Updated libmpq library slightly 
* Updated ScriptData/ContentData comments in Script library. 
* Updated VS2010/13 projects for scripts 
* Use proper path in usage examples. 
* Various 64Bit Build fixes 
* When consuming potions not usable by their class, characters will now to told they can't
* Windows service handler / ACE compatibility fixed.



0.18.1 (2013-11-02)
-------------------
Major changes for this build which require your attention when upgrading include
awesome things such as these:

* The *mangos-zero* build system has been overhauled, and we are now using CMake
  only. For Linux and FreeBSD users this means you can *always* use packages as
  provided by your distribution, and for Windows users this means you'll now
  have to download and install dependencies just once.
  We recommend that our Windows users pick up pre-built dependencies from
  [GNUWin32](http://gnuwin32.sourceforge.net/).
* The tools for map extraction and generation from the game client are finally
  first class citizens when you build *mangos-zero*, and will be built, too.
* The `genrevision` application has been removed from the build. Revision data
  and build information is now extracted via [Git](http://git-scm.com/) only.
* SOAP bindings for the world server are now optional, and will be disabled by
  default when building *mangos-zero*. If you need them, there is a CMake switch
  available to enable the bindings.
* The output given by all map tools has been cleaned up, and will now give you
  useful information such as the map version, or complete usage instructions.
  Pass the `--help` parameter, and any map tool will provide usage instructions!
* Documentation has been rewritten and converted to **Markdown** format, which
  is readable and converts nicely to HTML when viewing in the repository browser.
* Documentation has been added for all map tools including usage instructions
  and examples.
* Player movement has been rewritten, and now factors in possible issues such as
  lag when sending out character movement. This also means, looting when moving
  is no longer possible, and will be canceled.
* Looting in groups has been corrected, and you should now be able to use round
  robin, master looter, free for all and need before greed looting.
* EventAI is now more verbose, and will validate targets for commands upon server
  start-up. It's very likely that you will see many more errors now. Additionally
  the `npc aiinfo` command will display more useful info.
* **ScriptDev2** has been merged into the server repository! You do not need to
  make a clone, and *may need to delete* previously checkouts of the scripts
  repository. This also means, *ScriptDev2* will now always be built when you
  build the *mangos-zero* server.

Also numerous minor fixes and improvements have been added, such as:

* Using potions for power types not used by a class will now raise the correct
  error messages, e.g. Warriors can no longer consume Mana potions.
* Hunter pets will receive full experience when their masters are grouped.
* Mobs fleeing will do so now in normal speed, instead of crazy speed.
* The world server will now provide improved, readable output on start-up, and
  less confusing messages for identical issues.
* In-game commands `goname` and `namego` have been replaced with `appear` and
  `summon`. If you happen to find other commands with weird naming, let us know!
* We have done extensive house-keeping and removed many TBC specific code parts,
  and replaced TBC specific values with the proper vanilla WoW counterparts.
  This includes the TBC spell modifiers, which now have been dropped and are no
  longer available.
* Unprivileged player accounts will no longer be able to execute mangos dot
  commands in the in-game chat. If you need this, enable `PlayerCommands` in
  the mangosd configuration. The default setting is off.

0.18.0 (2013-08-31)
-------------------
Major changes for this build which require your attention when upgrading include
awesome things such as these:

* A build fix for FreeBSD has been added, thanks to @bels. *mangos-zero* should
  now successfully build again.
* In-game channels *Local defense* and zone channels no longer have characters as
  owners.
* Creature emotes have been fixed, and work again.
* Weather updates for zones without any defined weather have been fixed. A zone
  without weather will no longer cause the client to play random sounds.
* Spell linking based on conditions has been added. The table `spell_linked`
  now allows you to cast additional spells when a spell was cast based on a set
  of conditions.
* The table `scripted_event_id` has been renamed to `scripted_event`.
* `dbscripts_...` received a new command: `SCRIPT_COMMAND_TURN_TO` allows to
  turn creatures towards a target.
* EventAI received a new action: `ACTION_T_SUMMON_UNIQUE` which allows to
  summon a unique creature, which means the summon target can only be summoned
  once.
* The Scripting API has been streamlined to use more consistent function naming
  for all exported functions.

The following highlights the changes to the Scripting API:

* `isVendor` renamed to `IsVendor`
* `isTrainer` renamed to `IsTrainer`
* `isQuestGiver` renamed to `IsQuestGiver`
* `isGossip` renamed to `IsGossip`
* `isTaxi` renamed to `IsTaxi`
* `isGuildMaster` renamed to `IsGuildMaster`
* `isBattleMaster` renamed to `IsBattleMaster`
* `isBanker` renamed to `IsBanker`
* `isInnkeeper` renamed to `IsInnkeeper`
* `isSpiritHealer` renamed to `IsSpiritHealer`
* `isSpiritGuide` renamed to `IsSpiritGuide`
* `isTabardDesigner` renamed to `IsTabardDesigner`
* `isAuctioneer` renamed to `IsAuctioneer`
* `isArmorer` renamed to `IsArmorer`
* `isServiceProvider` renamed to `IsServiceProvider`
* `isSpiritService` renamed to `IsSpiritService`
* `isInCombat` renamed to `IsInCombat`
* `isFeared` renamed to `IsFeared`
* `isInRoots` renamed to `IsInRoots`
* `isFrozen` renamed to `IsFrozen`
* `isTargetableForAttack` renamed to `IsTargetableForAttack`
* `isAlive` renamed to `IsAlive`
* `isDead` renamed to `IsDead`
* `getDeathState` renamed to `GetDeathState`
* `isCharmedOwnedByPlayerOrPlayer` renamed to `IsCharmedOwnedByPlayerOrPlayer`
* `isCharmed` renamed to `IsCharmed`
* `isVisibleForOrDetect` renamed to `IsVisibleForOrDetect`
* `canDetectInvisibilityOf` renamed to `CanDetectInvisibilityOf`
* `isVisibleForInState` renamed to `IsVisibleForInState`
* `isInvisibleForAlive` renamed to `IsInvisibleForAlive`
* `getThreatManager` renamed to `GetThreatManager`
* `addHatedBy` renamed to `AddHatedBy`
* `removeHatedBy` renamed to `RemoveHatedBy`
* `getHostileRefManager` renamed to `GetHostileRefManager`
* `setTransForm` renamed to `SetTransform`
* `getTransForm` renamed to `GetTransform`
* `isHover` renamed to `IsHover`
* `addFollower` renamed to `AddFollower`
* `removeFollower` renamed to `RemoveFollower`
* `propagateSpeedChange` renamed to `PropagateSpeedChange`

Also numerous minor fixes and improvements have been added, such as:

* We have continued our research on client data, and resolved another batch of
  unknown variables and flags with their proper values including spell families
  and item classes.
* A few compile time warnings have been resolved, as well as a number of
  possible security issues, thanks to @Coverity scanning service for OS
  projects!
* Spells rewarding spells, and/or casting spells on reward will now cast the
  proper spell. Also quest givers will be able to cast spells ignoring Mana
  requirements when they are out of combat on reward.
* Documentation for many parts of the source code has been extended.
* An issue in the realm list server has been fixed, where it would loose the
  connection to MySQL.
* When casting [Soothe Animal](http://www.wowhead.com/spell=2908), the level
  of the targeted creature will be checked.
* The map extractor build for Linux was fixed, and builds properly again.
* A fix for applying speed for fleeing creatures was added.
