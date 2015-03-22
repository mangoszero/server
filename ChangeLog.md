MaNGOS Zero Changelog
=====================
This change log references the relevant changes (bug and security fixes) done
in recent versions.

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
