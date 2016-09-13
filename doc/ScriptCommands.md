Database script processing
==========================
In addition to *EventAI*, *mangos* provides script processing for various
types of game content. These scripts can be executed when creatures move or die,
when events are executed, and game object spawns/templates are used, on gossip
menu selections, when starting and completing quests, and when casting spells.

Database structure
------------------
All database scripts are stored in the `db_scripts` table in the world database

`script_guid`
-------------
Unique ID for the script, used as primary key in the table

`script_type`
--------------
Type of script. The values are:
* DBS_ON_QUEST_START(0): scripts which are executed at the quest taking
* DBS_ON_QUEST_END(1): scripts which are executed when the quest is rewarded
* DBS_ON_GOSSIP(2): scripts which are executed when player selects an item from a gossip NPC/GO
* DBS_ON_CREATURE_MOVEMENT(3): scripts executed when a creature passes through waypoints,
* DBS_ON_CREATURE_DEATH(4): as the name says
* DBS_ON_SPELL(5): scripts executed when a spell is cast (see what kind of spells below)
* DBS_ON_GO_USE(6): scripts executed when a particular gameobject (one from `gameobject` table) is "used" (i.e. opening/closing a door)
* DBS_ON_GOT_USE(7): scripts executed when a gameobject having the entry specified in `gameobject_template` is used
* DBS_ON_EVENT(8): scripts executed when an event occurs (like passing through taxi/transport nodes, spells with effect 61, gameobject_template data)

`id` column
-----------
Has different meanings, depending of script type
* type 0: DB project self defined id (generally quest entry)
* type 1: DB project self defined id (generally quest entry)
* type 2: DB project self defined id
* type 3: DB project self defined id
* type 4: Creature entry
* type 5: Spell id
* type 6: Game object guid
* type 7: Game object entry
* type 8: Event id

`delay` column
--------------
Delay in seconds. Defines the order in which steps are executed.

`command` column
----------------
The action to execute.

`datalong` columns
------------------
2 multipurpose fields, storing raw data as unsigned integer values.

`buddy_entry` column
--------------------
1 field to store the entry of a "buddy". Depending on the command used this can
can point to a game object template or/and creature template entry.

`search_radius` column
----------------------
Range in which the buddy defined in `buddy_entry` will be searched. In case of
`SCRIPT_FLAG_BUDDY_BY_GUID` this field is the buddy's guid!

`data_flags` column
-------------------
Field which holds a combination of these flags:

Value | Name                            | Notes
----- | ------------------------------- | ------------------------------------
1     | SCRIPT_FLAG_BUDDY_AS_TARGET     |
2     | SCRIPT_FLAG_REVERSE_DIRECTION   |
4     | SCRIPT_FLAG_SOURCE_TARGETS_SELF |
8     | SCRIPT_FLAG_COMMAND_ADDITIONAL  | (Only for some commands possible)
16    | SCRIPT_FLAG_BUDDY_BY_GUID       | (Interpret search_radius as buddy's guid)
32    | SCRIPT_FLAG_BUDDY_IS_PET        | (Do not search for an NPC, but for a pet)
64    | SCRIPT_FLAG_BUDDY_IS_DESPAWNED  | (Search for dead/despawned creatures)

`dataint` columns
-----------------
4 multipurpose fields, storing raw data as signed integer values.

*Note*: used for text ids SCRIPT_COMMAND_TALK (0),
      for emote ids in SCRIPT_COMMAND_EMOTE (1),
      for spell ids in SCRIPT_COMMAND_CAST_SPELL (15)
      as waittime with SCRIPT_COMMAND_TERMINATE_SCRIPT (31)
      as equipment ID's with SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS (42)

`x`, `y`, `z` and `o` columns
-----------------------------
Map coordinates for commands that require coordinates

Origin of script and source/target in scripts
---------------------------------------------

* type 0: `quest_template` with source: quest giver (creature/GO)
  and target: player.
* type 1: `quest_template` with source: quest taker (creature/GO)
  and target: player.
* type 2: `gossip_menu_option`, `gossip_menu` with source: creature
  and target: player in case of NPC-Gossip, or with source: player and target:
  game object in case of GO-Gossip.
* type 3: `creature_movement` and `creature_movement_template`.
  Source: creature. Target: creature
* type 4: Creature death.
  Source: creature. Target: Unit (player/creature)
* type 5: spell with effect 77(SCRIPT_EFFECT), spells with effect 3
  (DUMMY), spells with effect 64 (TRIGGER_SPELL, with non-existing triggered spell)
  having source: caster: and target: target of spell (Unit).
* type 6, 7: game object use with
  source: user: and target: game object.
* type 8: Flight path with source: player and target: player,
  transport path with source: transport game object and target transport game
  object, `gameobject_template` with source: user (player/creature) and target:
  game object, spells having effect 61 with source: caster and target: target.

Buddy concept
-------------
Commands except the ones requiring a player (like KILL_CREDIT) have support
for the buddy concept. This means that if an entry for `buddy_entry` is
provided, aside from source and target as listed above also a "buddy" is
available.

Which one on the three (`originalSource`, `originalTarget`, `buddy`) will
be used in the command, depends on the `data_flags` settings.

Note that some commands (like EMOTE) use only the resulting source for an
action.

Possible combinations of the flags:

* `SCRIPT_FLAG_BUDDY_AS_TARGET`: 0x01
* `SCRIPT_FLAG_REVERSE_DIRECTION`: 0x02
* `SCRIPT_FLAG_SOURCE_TARGETS_SELF`: 0x04

are

* 0: originalSource / buddyIfProvided  ->  originalTarget
* 1: originalSource  ->  buddy
* 2: originalTarget  ->  originalSource / buddyIfProvided
* 3: buddy  ->  originalSource
* 4: originalSource / buddyIfProvided  ->  originalSource / buddyIfProvided
* 5: originalSource  ->  originalSource
* 6: originalTarget  ->  originalTarget
* 7: buddy  ->  buddy

where `A -> B` means that the command is executed from A with B as target.

Commands and their parameters
-----------------------------

<table border='1' cellspacing='1' cellpadding='3' bgcolor='#f0f0f0'>
<tr bgcolor='#f0f0ff'>
<th><b>ID</b></th>
<th align='left'><b>Name</b></th>
<th align='left'><b>Parameters</b></th>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>0</td><td align='left' valign='middle'>SCRIPT_COMMAND_TALK</td><td align='left' valign='middle'>resultingSource = WorldObject, resultingTarget = Unit/none, `dataint` = text entry from db_script_string -table. `dataint2`-`dataint4` optionally, for random selection of text</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>1</td><td align='left' valign='middle'>SCRIPT_COMMAND_EMOTE</td><td align='left' valign='middle'>resultingSource = Unit, resultingTarget = Unit/none, `datalong` = emote_id, dataint1-dataint4 = optionally for random selection of emote</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>2</td><td align='left' valign='middle'>SCRIPT_COMMAND_FIELD_SET</td><td align='left' valign='middle'>source = any, `datalong` = field_id, `datalong2` = field value</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>3</td><td align='left' valign='middle'>SCRIPT_COMMAND_MOVE_TO</td><td align='left' valign='middle'>resultingSource = Creature. If position is very near to current position, or x=y=z=0, then only orientation is changed. `datalong2` = travel_speed*100 (use 0 for creature default movement). `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: teleport unit to position `x`/`y`/`z`/`o`</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>4</td><td align='left' valign='middle'>SCRIPT_COMMAND_FLAG_SET</td><td align='left' valign='middle'>source = any. `datalong` = field_id, `datalong2` = bit mask</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>5</td><td align='left' valign='middle'>SCRIPT_COMMAND_FLAG_REMOVE</td><td align='left' valign='middle'>source = any. `datalong` = field_id, `datalong2` = bit mask</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>6</td><td align='left' valign='middle'>SCRIPT_COMMAND_TELEPORT_TO</td><td align='left' valign='middle'>source or target with Player. `datalong` = map_id, x/y/z</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>7</td><td align='left' valign='middle'>SCRIPT_COMMAND_QUEST_EXPLORED</td><td align='left' valign='middle'>one from source or target must be Player, another GO/Creature. `datalong` = quest_id, `datalong2` = distance or 0</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>8</td><td align='left' valign='middle'>SCRIPT_COMMAND_KILL_CREDIT</td><td align='left' valign='middle'>source or target with Player. `datalong` = creature entry, or 0; If 0 the entry of the creature source or target is used, `datalong2` = bool (0=personal credit, 1=group credit)</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>9</td><td align='left' valign='middle'>SCRIPT_COMMAND_RESPAWN_GO</td><td align='left' valign='middle'>source = any, target = any. `datalong`=db_guid (can be skipped for buddy), `datalong2` = despawn_delay</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>10</td><td align='left' valign='middle'>SCRIPT_COMMAND_TEMP_SUMMON_CREATURE</td><td align='left' valign='middle'>source = any<br/>target = any<br/>`datalong` = creature entry<br/>`datalong2` = despawn_delay<br/>`data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: summon as active object<br/>`dataint` = bool (setRun); 0 = off (default), 1 = on</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>11</td><td align='left' valign='middle'>SCRIPT_COMMAND_OPEN_DOOR</td><td align='left' valign='middle'>source = any. `datalong` = db_guid (can be skipped for buddy), `datalong2` = reset_delay</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>12</td><td align='left' valign='middle'>SCRIPT_COMMAND_CLOSE_DOOR</td><td align='left' valign='middle'>source = any. `datalong` = db_guid (can be skipped for buddy), `datalong2` = reset_delay</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>13</td><td align='left' valign='middle'>SCRIPT_COMMAND_ACTIVATE_OBJECT</td><td align='left' valign='middle'>source = unit, target=GO.</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>14</td><td align='left' valign='middle'>SCRIPT_COMMAND_REMOVE_AURA</td><td align='left' valign='middle'>resultingSource = Unit. `datalong` = spell_id</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>15</td><td align='left' valign='middle'>SCRIPT_COMMAND_CAST_SPELL</td><td align='left' valign='middle'>resultingSource = Unit, cast spell at resultingTarget = Unit. `datalong` = spell id, `dataint1`-`dataint4` optional. If some of these are set to a spell id, a random spell out of datalong, datint1, ..,dataintX is cast., `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: cast triggered</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>16</td><td align='left' valign='middle'>SCRIPT_COMMAND_PLAY_SOUND</td><td align='left' valign='middle'>source = any object, target=any/player. `datalong` = sound_id, `datalong2` (bit mask: 0/1=target-player, 0/2=with distance dependent, 0/4=map wide, 0/8=zone wide; so 1 + 2 = 3 is target with distance dependent)</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>17</td><td align='left' valign='middle'>SCRIPT_COMMAND_CREATE_ITEM</td><td align='left' valign='middle'>source or target must be player. `datalong` = item entry, `datalong2` = amount</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>18</td><td align='left' valign='middle'>SCRIPT_COMMAND_DESPAWN_SELF</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = despawn delay</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>19</td><td align='left' valign='middle'>SCRIPT_COMMAND_PLAY_MOVIE</td><td align='left' valign='middle'>target can only be a player. `datalong` = movie id</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>20</td><td align='left' valign='middle'>SCRIPT_COMMAND_MOVEMENT</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = MovementType (0:idle, 1:random or 2:waypoint), `datalong2` = wanderDistance (for random movement), `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: RandomMovement around current position</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>21</td><td align='left' valign='middle'>SCRIPT_COMMAND_SET_ACTIVEOBJECT</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = bool 0=off, 1=on</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>22</td><td align='left' valign='middle'>SCRIPT_COMMAND_SET_FACTION</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = factionId OR 0 to restore original faction from creature_template, `datalong2` = enum TemporaryFactionFlags</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>23</td><td align='left' valign='middle'>SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = creature entry/modelid (depend on data_flags) OR 0 to demorph, `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: use datalong value as modelid explicit</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>24</td><td align='left' valign='middle'>SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = creature entry/modelid (depend on data_flags) OR 0 to dismount, `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL: use datalong value as modelid explicit</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>25</td><td align='left' valign='middle'>SCRIPT_COMMAND_SET_RUN</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = bool 0=off, 1=on</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>26</td><td align='left' valign='middle'>SCRIPT_COMMAND_ATTACK_START</td><td align='left' valign='middle'>resultingSource = Creature, resultingTarget = Unit.</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>27</td><td align='left' valign='middle'>SCRIPT_COMMAND_GO_LOCK_STATE</td><td align='left' valign='middle'>resultingSource = GO. `datalong` = flag_go_lock = 0x01, flag_go_unlock = 0x02, flag_go_nonInteract = 0x04, flag_go_interact = 0x08</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>28</td><td align='left' valign='middle'>SCRIPT_COMMAND_STAND_STATE</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = stand state (enum UnitStandStateType)</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>29</td><td align='left' valign='middle'>SCRIPT_COMMAND_MODIFY_NPC_FLAGS</td><td align='left' valign='middle'>resultingSource = Creature. `datalong` = NPCFlags, `datalong2` = 0x00=toggle, 0x01=add, 0x02=remove</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>30</td><td align='left' valign='middle'>SCRIPT_COMMAND_SEND_TAXI_PATH</td><td align='left' valign='middle'>resultingTarget or Source must be Player. `datalong` = taxi path id</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>31</td><td align='left' valign='middle'>SCRIPT_COMMAND_TERMINATE_SCRIPT</td><td align='left' valign='middle'>`datalong` = search for npc entry if provided, `datalong2` = search distance, `!(data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)`: if npc not alive found, terminate script, `data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL`:if npc alive found, terminate script, `dataint` = change of waittime (MILLISECONDS) of a current waypoint movement type (negative values will decrease time)</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>32</td><td align='left' valign='middle'>SCRIPT_COMMAND_PAUSE_WAYPOINTS</td><td align='left' valign='middle'>resultingSource must be Creature. `datalong` = 0/1 unpause/pause waypoint movement</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>33</td><td align='left' valign='middle'>SCRIPT_COMMAND_RESERVED_1</td><td align='left' valign='middle'>reserved for 3.x and later. Do not use!</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>34</td><td align='left' valign='middle'>SCRIPT_COMMAND_TERMINATE_COND</td><td align='left' valign='middle'>`datalong` = condition_id, `datalong2` = fail-quest (if provided this quest will be failed for a player), `!(data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)`: terminate when condition is true, `data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL`:terminate when condition is false</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>35</td><td align='left' valign='middle'>SCRIPT_COMMAND_SEND_AI_EVENT_AROUND</td><td align='left' valign='middle'>resultingSource = Creature, resultingTarget = Unit, datalong = AIEventType - limited only to EventAI supported events, datalong2 = radius</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>36</td><td align='left' valign='middle'>SCRIPT_COMMAND_TURN_TO</td><td align='left' valign='middle'>resultingSource = Creature, resultingTarget = Unit/none.</td></tr>
<tr bgcolor='#FEFEFF'><td align='center' valign='middle'>37</td><td align='left' valign='middle'>SCRIPT_COMMAND_MOVE_DYNAMIC</td><td align='left' valign='middle'>Move resultingSource to a random point around resultingTarget or to resultingTarget. `resultingSource` = Creature, resultingTarget Worldobject. `datalong` = 0:Move resultingSource towards resultingTarget, `datalong` != 0: Move resultingSource to a random point between datalong2..datalong around resultingTarget. `orientation` != 0: Obtain a random point around resultingTarget in direction of orientation, `data_flags` & SCRIPT_FLAG_COMMAND_ADDITIONAL Obtain a point in direction of resTarget->GetOrientation + orientation for resTarget == resSource and orientation == 0 this will mean resSource moving forward.</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>38</td><td align='left' valign='middle'>SCRIPT_COMMAND_SEND_MAIL</td><td align='left' valign='middle'>Send a mail from resSource to resTarget. `resultingSource` = Creature OR NULL, resTarget must be Player, `datalong` = mailTemplateId, `datalong2`: AlternativeSenderEntry. Use as sender-Entry of the sent mail, `dataint1`: Delay (>= 0) in Seconds</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>39</td><td align='left' valign='middle'>SCRIPT_COMMAND_SET_FLY</td><td align='left' valign='middle'>Toggles the fly mode for a creature.<br/>* resultingSource = Creature<br/>datalong = 0(off) | 1 (on)<br/>* data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL = sets/unsets the flying animation</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>40</td><td align='left' valign='middle'>SCRIPT_COMMAND_DESPAWN_GO</td><td align='left' valign='middle'>* resultingTarget = GameObject. Not all gameobject types support this command</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>41</td><td align='left' valign='middle'>SCRIPT_COMMAND_RESPAWN</td><td align='left' valign='middle'>* resultingSource = Creature. Requires data_flags & SCRIPT_FLAG_BUDDY_IS_DESPAWNED to find dead or despawned targets</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>42</td><td align='left' valign='middle'>SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS</td><td align='left' valign='middle'>Change equipment slots of the resultingSource (Creature)<br/>* resultingSource = Creature<br/>* datalong = reset default (0 - false, 1 - true)<br/>* dataint = main hand slot<br/>* dataint2 = off handslot<br/>* dataint3 = ranged slot</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>43</td><td align='left' valign='middle'>SCRIPT_COMMAND_RESET_GO</td><td align='left' valign='middle'>Resets doors or buttons (GO type 0 or 1)<br/>* resultingTarget = GameObject</td></tr>
<tr bgcolor='#FFFFEE'><td align='center' valign='middle'>44</td><td align='left' valign='middle'>SCRIPT_COMMAND_UPDATE_TEMPLATE</td><td align='left' valign='middle'>Update creature entry of the resultingSource<br/>* resultingSource = creature<br/>* datalong = new creature entry. Must be different than the current one</td></tr>
</table>

TemporaryFactionFlags
---------------------
* `TEMPFACTION_NONE`: 0x00, when no flag is used in temporary faction change, faction will be persistent. It will then require manual change back to default/another faction when changed once
* `TEMPFACTION_RESTORE_RESPAWN`: 0x01, default faction will be restored at respawn
* `TEMPFACTION_RESTORE_COMBAT_STOP`: 0x02, ... at CombatStop() (happens at creature death, at evade or custom script among others)
* `TEMPFACTION_RESTORE_REACH_HOME`: 0x04, ... at reaching home in home movement (evade), if not already done at CombatStop()

The next flags allow to remove unit_flags combined with a faction change (also these flags will be reapplied when the faction is changed back)

* `TEMPFACTION_TOGGLE_NON_ATTACKABLE`: 0x08, remove UNIT_FLAG_NON_ATTACKABLE(0x02) when faction is changed (reapply when temp-faction is removed)
* `TEMPFACTION_TOGGLE_OOC_NOT_ATTACK`: 0x10, remove UNIT_FLAG_OOC_NOT_ATTACKABLE(0x100) when faction is changed (reapply when temp-faction is removed)
* `TEMPFACTION_TOGGLE_PASSIVE`       : 0x20, remove UNIT_FLAG_PASSIVE(0x200)
* `TEMPFACTION_TOGGLE_PACIFIED`      : 0x40, remove UNIT_FLAG_PACIFIED(0x20000) when faction is changed (reapply when temp-faction is removed)
* `TEMPFACTION_TOGGLE_NOT_SELECTABLE`: 0x80, remove UNIT_FLAG_NOT_SELECTABLE(0x2000000) when faction is changed (reapply when temp-faction is removed)
