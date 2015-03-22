Texts Documentation
===================
In order for scripts to have a centralized storage for texts, text tables have been
added to the database. Any script can access and use texts from these tables.

An additional table is available for custom scripts.

For each table ranges of valid identifiers have been define

* entry `-1` to `-999999`: reserved EventAI in *mangos*,
* entry `-1000000` to `-1999999`: script text entries,
* entry `-2000000` to `-2999999`: text entries for custom scripts,
* entry `-3000000` to `-3999999`: texts for scripted gossip texts.

Text entries not using identifiers from the defined ranges will result in startup
errors.

Database structure
------------------
`custom_texts`, `gossip_texts`, and `script_texts` share an indentical table
structure, thus making it very easy to add new text entries.

Field name      | Description
--------------- | --------------------------------------------------------------
entry           | A unique *negative* identifier to the text entry.
content_default | The default text to be displayed in English.
content_loc1    | Korean localization of `content_default`.
content_loc2    | French localization of `content_default`.
content_loc3    | German localization of `content_default`.
content_loc4    | Chinese localization of `content_default`.
content_loc5    | Taiwanese localization of `content_default`.
content_loc6    | Spanish Spain localization of `content_default`.
content_loc7    | Spanish Latin America localization of `content_default`.
content_loc8    | Russian localization of `content_default`.
sound           | A sound from SoundEntries.dbc to be played.
type            | Type of text (Say/Yell/Text emote/Whisper/Boss whisper/zone yell).
language        | A text language from Languages.dbc
emote           | An emote from Emotes.dbc. Only source of text will play this emote (not target, if target are defined in DoScriptText)
comment         | This is a comment using the Creature ID of NPC using it.

*Note*: `sound`, `type`, `language` and `emote` exist only in the tables
`script_texts` and `custom_texts`.

*Note*: Fields `content_loc1` to `content_loc8` are `NULL` values by default and
are handled by separate localization projects.

Text Types (`type`)
-------------------
Below is the list of current text types that texts tables can handle.

ID | Internal name          | Description
-- | ---------------------- | ----------------------------------
0  | CHAT_TYPE_SAY          | Displayed as a Say (Speech Bubble).
1  | CHAT_TYPE_YELL         | Displayed as a Yell (Red Speech Bubble) and usually has a matching Sound ID.
2  | CHAT_TYPE_TEXT_EMOTE   | Displayed as a text emote in orange in the chat log.
3  | CHAT_TYPE_BOSS_EMOTE   | Displayed as a text emote in orange in the chat log (Used only for specific Bosses).
4  | CHAT_TYPE_WHISPER      | Displayed as a whisper to the player in the chat log.
5  | CHAT_TYPE_BOSS_WHISPER | Displayed as a whisper to the player in the chat log (Used only for specific Bosses).
6  | CHAT_TYPE_ZONE_YELL    | Same as CHAT_TYPE_YELL but will display to all players in current zone.

Language Types (`language`)
---------------------------
This is the race language that the text is native to. Below is the list of
current language types that are allowed.

ID  | Internal Name | Description
--- | ------------- | --------------------------------------------------------
0   | UNIVERSAL     | Understood by *all* races.
1   | ORCISH        | Understood *only* by Horde races.
2   | DARNASSIAN    | Understood *only* by the Night Elf race.
3   | TAURAHE       | Understood *only* by the Tauren race.
6   | DWARVISH      | Understood *only* by the Dwarf race.
7   | COMMON        | Understood *only* by Alliance races.
8   | DEMONIC       | Understood *only* by the Demon race (Not Implemented).
9   | TITAN         | This language was used by Sargeras to speak with other Titians (Not Implemented).
10  | THALASSIAN    | Understood *only* by the Blood Elf race.
11  | DRACONIC      | Understood *only* by the Dragon race.
12  | KALIMAG       | Text will display as Kalimag (not readable by players, language of all elementals)
13  | GNOMISH       | Understood *only* by the Gnome race.
14  | TROLL         | Understood *only* by the Troll race.
33  | GUTTERSPEAK   | Understood *only* by the Undead race.
