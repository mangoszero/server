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
 * @file Chat.cpp
 * @brief Chat system implementation
 *
 * This file implements the chat system including:
 * - Message formatting and color codes
 * - Shift-link parsing (item, spell, quest links)
 * - Channel message routing
 * - Whisper, say, yell, emote handling
 * - GM command parsing and execution
 * - Language filtering
 *
 * The chat system supports various message types with different
 * visibility ranges and formatting requirements.
 *
 * @see ChatHandler for command handling
 * @see Channel for channel chat
 */



#include "Chat.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "AccountMgr.h"
#include "DBCStores.h"
#include "Util.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectGuid.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "CommandMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * Function skip all whitespaces in args string
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *             allowed NULL string pointer stored in *args
 */
void ChatHandler::SkipWhiteSpaces(char** args)
{
    if (!*args)
    {
        return;
    }

    while (isWhiteSpace(**args))
    {
        ++(*args);
    }
}

/**
 * Function extract to val arg signed integer value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original value unmodified
 * @return     true if value extraction successful
 */
bool  ChatHandler::ExtractInt32(char** args, int32& val)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    long valRaw = strtol(*args, &tail, 10);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (*tail)                                         // some not whitespace symbol
    {
        return false;                                        // args not modified and can be re-parsed
    }

    if (valRaw < std::numeric_limits<int32>::min() || valRaw > std::numeric_limits<int32>::max())
    {
        return false;
    }

    // value successfully extracted
    val = int32(valRaw);
    *args = tail;
    return true;
}

/**
 * Function extract to val arg optional signed integer value or use default value. Fail if extracted not signed integer.
 *
 * @param args    variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool  ChatHandler::ExtractOptInt32(char** args, int32& val, int32 defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractInt32(args, val);
}

/**
 * Function extract to val arg unsigned integer value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original value unmodified
 * @param base set used base for extracted value format (10 for decimal, 16 for hex, etc), 0 let auto select by system internal function
 * @return     true if value extraction successful
 */
bool  ChatHandler::ExtractUInt32Base(char** args, uint32& val, uint32 base)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    unsigned long valRaw = strtoul(*args, &tail, base);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (tail && *tail)                                 // some not whitespace symbol
    {
        return false;                                        // args not modified and can be re-parsed
    }

    if (valRaw > std::numeric_limits<uint32>::max())
    {
        return false;
    }

    // value successfully extracted
    val = uint32(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

/**
 * Function extract to val arg optional unsigned integer value or use default value. Fail if extracted not unsigned integer.
 *
 * @param args    variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool  ChatHandler::ExtractOptUInt32(char** args, uint32& val, uint32 defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractUInt32(args, val);
}

/**
 * Function extract to val arg float value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original value unmodified
 * @return     true if value extraction successful
 */
bool  ChatHandler::ExtractFloat(char** args, float& val)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    double valRaw = strtod(*args, &tail);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (tail && *tail)                                 // some not whitespace symbol
    {
        return false;                                        // args not modified and can be re-parsed
    }

    // value successfully extracted
    val = float(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

/**
 * Function extract to val arg optional float value or use default value. Fail if extracted not float.
 *
 * @param args    variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool  ChatHandler::ExtractOptFloat(char** args, float& val, float defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractFloat(args, val);
}

/**
 * Function extract name-like string (from non-numeric or special symbol until whitespace)
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param lit  optional explicit literal requirement. function fail if literal is not starting substring of lit.
 *             Note: function in same way fail if no any literal or literal not fit in this case. Need additional check for select specific fail case
 * @return     name/number-like string without whitespaces, or NULL if args empty or not appropriate content.
 */
char* ChatHandler::ExtractLiteralArg(char** args, char const* lit /*= NULL*/)
{
    if (!*args || !** args)
    {
        return NULL;
    }

    char* head = *args;

    // reject quoted string or link (|-started text)
    switch (head[0])
    {
        // reject quoted string
        case '[': case '\'': case '"':
            return NULL;
        // reject link (|-started text)
        case '|':
            // client replace all | by || in raw text
            if (head[1] != '|')
            {
                return NULL;
            }
            ++head;                                         // skip one |
            break;
        default: break;
    }

    if (lit)
    {
        int l = strlen(lit);

        int largs = 0;
        while (head[largs] && !isWhiteSpace(head[largs]))
        {
            ++largs;
        }

        if (largs < l)
        {
            l = largs;
        }

        int diff = strncmp(head, lit, l);

        if (diff != 0)
        {
            return NULL;
        }

        if (head[l] && !isWhiteSpace(head[l]))
        {
            return NULL;
        }

        char* arg = head;

        if (head[l])
        {
            head[l] = '\0';

            head += l + 1;

            *args = head;
        }
        else
        {
            *args = head + l;
        }

        SkipWhiteSpaces(args);
        return arg;
    }

    char* name = strtok(head, " ");

    char* tail = strtok(NULL, "");

    *args = tail ? tail : (char*)"";                        // *args don't must be NULL

    SkipWhiteSpaces(args);

    return name;
}

/**
 * Function extract quote-like string (any characters guarded by some special character, in our cases ['")
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     quote-like string, or NULL if args empty or not appropriate content.
 */
char* ChatHandler::ExtractQuotedArg(char** args, bool asis /*= false*/)
{
    if (!*args || !** args)
    {
        return NULL;
    }

    if (**args != '\'' &&**  args != '"' &&**  args != '[')
    {
        return NULL;
    }

    char guard = (*args)[0];

    if (guard == '[')
    {
        guard = ']';
    }

    char* tail = (*args) + 1;                               // start scan after first quote symbol
    char* head = asis ? *args : tail;                       // start arg

    while (*tail && *tail != guard)
    {
        ++tail;
    }

    if (!*tail || (tail[1] && !isWhiteSpace(tail[1])))      // fail
    {
        return NULL;
    }

    if (!tail[1])                                           // quote is last char in string
    {
        if (!asis)
        {
            *tail = '\0';
        }
    }
    else                                                    // quote isn't last char
    {
        if (asis)
        {
            ++tail;
        }

        *tail = '\0';
    }

    *args = tail + 1;

    SkipWhiteSpaces(args);

    return head;
}

/**
 * Function extract quote-like string or literal if quote not detected
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     quote/literal string, or NULL if args empty or not appropriate content.
 */
char* ChatHandler::ExtractQuotedOrLiteralArg(char** args, bool asis /*= false*/)
{
    char* arg = ExtractQuotedArg(args, asis);
    if (!arg)
    {
        arg = ExtractLiteralArg(args);
    }
    return arg;
}

/**
 * Function extract on/off literals as boolean values
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original value unmodified
 * @return     true at success
 */
bool  ChatHandler::ExtractOnOff(char** args, bool& value)
{
    char* arg = ExtractLiteralArg(args);
    if (!arg)
    {
        return false;
    }

    if (strncmp(arg, "on", 3) == 0)
    {
        value = true;
    }
    else if (strncmp(arg, "off", 4) == 0)
    {
        value = false;
    }
    else
    {
        return false;
    }

    return true;
}

/**
 * Function extract shift-link-like string (any characters guarded by | and |h|r with some additional internal structure check)
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *
 * @param linkTypes  optional NULL-terminated array of link types, shift-link must fit one from link type from array if provided or extraction fail
 *
 * @param found_idx  if not NULL then at return index in linkTypes that fit shift-link type, if extraction fail then non modified
 *
 * @param keyPair    if not NULL then pointer to 2-elements array for return start and end pointer for found key
 *                   if extraction fail then non modified
 *
 * @param somethingPair then pointer to 2-elements array for return start and end pointer if found.
 *                   if not NULL then shift-link must have data field, if extraction fail then non modified
 *
 * @return     shift-link-like string, or NULL if args empty or not appropriate content.
 */
char* ChatHandler::ExtractLinkArg(char** args, char const* const* linkTypes /*= NULL*/, int* foundIdx /*= NULL*/, char** keyPair /*= NULL*/, char** somethingPair /*= NULL*/)
{
    if (!*args || !** args)
    {
        return NULL;
    }

    // skip if not linked started or encoded single | (doubled by client)
    if ((*args)[0] != '|' || (*args)[1] == '|')
    {
        return NULL;
    }

    // |color|Hlinktype:key:data...|h[name]|h|r

    char* head = *args;

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r
    // or
    // [name] Shift-click form |linkType:key|h[name]|h|r

    // |color|Hlinktype:key:data...|h[name]|h|r

    char* tail = (*args) + 1;                               // skip |

    if (*tail != 'H')                                       // skip color part, some links can not have color part
    {
        while (*tail && *tail != '|')
        {
            ++tail;
        }

        if (!*tail)
        {
            return NULL;
        }

        // |Hlinktype:key:data...|h[name]|h|r

        ++tail;                                             // skip |
    }

    // Hlinktype:key:data...|h[name]|h|r

    if (*tail != 'H')
    {
        return NULL;
    }

    int linktype_idx = 0;

    if (linkTypes)                                          // check link type if provided
    {
        // check linktypes (its include H in name)
        for (; linkTypes[linktype_idx]; ++linktype_idx)
        {
            // exactly string with follow : or |
            int l = strlen(linkTypes[linktype_idx]);
            if (strncmp(tail, linkTypes[linktype_idx], l) == 0 &&
                (tail[l] == ':' || tail[l] == '|'))
            {
                break;
            }
        }

        // is search fail?
        if (!linkTypes[linktype_idx])                       // NULL terminator in last element
        {
            return NULL;
        }

        tail += strlen(linkTypes[linktype_idx]);            // skip linktype string

        // :key:data...|h[name]|h|r

        if (*tail != ':')
        {
            return NULL;
        }
    }
    else
    {
        while (*tail && *tail != ':')                       // skip linktype string
        {
            ++tail;
        }

        if (!*tail)
        {
            return NULL;
        }
    }

    ++tail;

    // key:data...|h[name]|h|r
    char* keyStart = tail;                                  // remember key start for return

    while (*tail && *tail != '|' && *tail != ':')
    {
        ++tail;
    }

    if (!*tail)
    {
        return NULL;
    }

    char* keyEnd = tail;                                    // remember key end for truncate

    // |h[name]|h|r or :something...|h[name]|h|r

    char* somethingStart = tail + 1;
    char* somethingEnd   = tail + 1;                        // will updated later if need

    if (*tail == ':' && somethingPair)                      // optional data extraction
    {
        // :something...|h[name]|h|r
        ++tail;

        // something|h[name]|h|r or something:something2...|h[name]|h|r

        while (*tail && *tail != '|' && *tail != ':')
        {
            ++tail;
        }

        if (!*tail)
        {
            return NULL;
        }

        somethingEnd = tail;                                // remember data end for truncate
    }

    // |h[name]|h|r or :something2...|h[name]|h|r

    while (*tail && (*tail != '|' || *(tail + 1) != 'h'))   // skip ... part if exist
    {
        ++tail;
    }

    if (!*tail)
    {
        return NULL;
    }

    // |h[name]|h|r

    tail += 2;                                              // skip |h

    // [name]|h|r
    if (*tail != '[')
    {
        return NULL;
    }

    while (*tail && (*tail != ']' || *(tail + 1) != '|'))   // skip name part
    {
        ++tail;
    }

    tail += 2;                                              // skip ]|

    // h|r
    if (*tail != 'h' || *(tail + 1) != '|')
    {
        return NULL;
    }

    tail += 2;                                              // skip h|

    // r
    if (*tail != 'r' || (*(tail + 1) && !isWhiteSpace(*(tail + 1))))
    {
        return NULL;
    }

    ++tail;                                                 // skip r

    // success

    if (*tail)                                              // truncate all link string
    {
        *(tail++) = '\0';
    }

    if (foundIdx)
    {
        *foundIdx = linktype_idx;
    }

    if (keyPair)
    {
        keyPair[0] = keyStart;
        keyPair[1] = keyEnd;
    }

    if (somethingPair)
    {
        somethingPair[0] = somethingStart;
        somethingPair[1] = somethingEnd;
    }

    *args = tail;

    SkipWhiteSpaces(args);

    return head;
}

/**
 * Function extract name/number/quote/shift-link-like string
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     extracted arg string, or NULL if args empty or not appropriate content.
 */
char* ChatHandler::ExtractArg(char** args, bool asis /*= false*/)
{
    if (!*args || !** args)
    {
        return NULL;
    }

    char* arg = ExtractQuotedOrLiteralArg(args, asis);
    if (!arg)
    {
        arg = ExtractLinkArg(args);
    }

    return arg;
}

/**
 * Function extract name/quote/number/shift-link-like string, and return it if args have more non-whitespace data
 *
 * @param args variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *             if args have only single arg then args still pointing to this arg (unmodified pointer)
 * @return     extracted string, or NULL if args empty or not appropriate content or have single arg totally.
 */
char* ChatHandler::ExtractOptNotLastArg(char** args)
{
    char* arg = ExtractArg(args, true);

    // have more data
    if (*args &&**  args)
    {
        return arg;
    }

    // optional name not found
    *args = arg ? arg : (char*)"";                          // *args don't must be NULL

    return NULL;
}

/**
 * Function extract data from shift-link "|color|LINKTYPE:RETURN:SOMETHING1|h[name]|h|r if linkType == LINKTYPE
 * It also extract literal/quote if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args still pointing to this arg (unmodified pointer)
 *
 * @param linkType   shift-link must fit by link type to this arg value or extraction fail
 *
 * @param something1 if not NULL then shift-link must have data field and it returned into this arg
 *                   if extraction fail then non modified
 *
 * @return           extracted key, or NULL if args empty or not appropriate content or not fit to linkType.
 */
char* ChatHandler::ExtractKeyFromLink(char** text, char const* linkType, char** something1 /*= NULL*/)
{
    char const* linkTypes[2];
    linkTypes[0] = linkType;
    linkTypes[1] = NULL;

    int foundIdx;

    return ExtractKeyFromLink(text, linkTypes, &foundIdx, something1);
}

/**
 * Function extract data from shift-link "|color|LINKTYPE:RETURN:SOMETHING1|h[name]|h|r if LINKTYPE in linkTypes array
 * It also extract literal/quote if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args still pointing to this arg (unmodified pointer)
 *
 * @param linkTypes  NULL-terminated array of link types, shift-link must fit one from link type from array or extraction fail
 *
 * @param found_idx  if not NULL then at return index in linkTypes that fit shift-link type, for non-link case return -1
 *                   if extraction fail then non modified
 *
 * @param something1 if not NULL then shift-link must have data field and it returned into this arg
 *                   if extraction fail then non modified
 *
 * @return           extracted key, or NULL if args empty or not appropriate content or not fit to linkType.
 */
char* ChatHandler::ExtractKeyFromLink(char** text, char const* const* linkTypes, int* found_idx, char** something1 /*= NULL*/)
{
    // skip empty
    if (!*text || !** text)
    {
        return NULL;
    }

    // return non link case
    char* arg = ExtractQuotedOrLiteralArg(text);
    if (arg)
    {
        if (found_idx)
        {
            *found_idx = -1;                                 // special index case
        }

        return arg;
    }

    char* keyPair[2];
    char* somethingPair[2];

    arg = ExtractLinkArg(text, linkTypes, found_idx, keyPair, something1 ? somethingPair : NULL);
    if (!arg)
    {
        return NULL;
    }

    *keyPair[1] = '\0';                                     // truncate key string

    if (something1)
    {
        *somethingPair[1] = '\0';                           // truncate data string
        *something1 = somethingPair[0];
    }

    return keyPair[0];
}

/**
 * Function extract uint32 key from shift-link "|color|LINKTYPE:RETURN|h[name]|h|r if linkType == LINKTYPE
 * It also extract direct number if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args still pointing to this arg (unmodified pointer)
 *
 * @param linkType   shift-link must fit by link type to this arg value or extraction fail
 *
 * @param value      store result value at success return, not modified at fail
 *
 * @return           true if extraction succesful
 */
bool ChatHandler::ExtractUint32KeyFromLink(char** text, char const* linkType, uint32& value)
{
    char* arg = ExtractKeyFromLink(text, linkType);
    if (!arg)
    {
        return false;
    }

    return ExtractUInt32(&arg, value);
}

/**
 * @brief Retrieves a game object on the current map by low GUID and entry.
 *
 * @param lowguid The game object low GUID.
 * @param entry The game object entry identifier.
 * @return GameObject* The matching game object, or NULL if not found.
 */
GameObject* ChatHandler::GetGameObjectWithGuid(uint32 lowguid, uint32 entry)
{
    if (!m_session)
    {
        return NULL;
    }

    Player* pl = m_session->GetPlayer();

    return pl->GetMap()->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, entry, lowguid));
}

enum SpellLinkType
{
    SPELL_LINK_RAW     = -1,                                // non-link case
    SPELL_LINK_SPELL   = 0,
    SPELL_LINK_TALENT  = 1,
    SPELL_LINK_ENCHANT = 2,
};

static char const* const spellKeys[] =
{
        "Hspell",                                               // normal spell
        "Htalent",                                              // talent spell
        "Henchant",                                             // enchanting recipe spell
    NULL
};

/**
 * @brief Extracts a spell id from a raw argument or supported spell-related link.
 *
 * @param text The argument text pointer to parse.
 * @return uint32 The extracted spell id, or 0 on failure.
 */
uint32 ChatHandler::ExtractSpellIdFromLink(char** text)
{
    // number or [name] Shift-click form |color|Henchant:recipe_spell_id|h[prof_name: recipe_name]|h|r
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    // number or [name] Shift-click form |color|Htalent:talent_id,rank|h[name]|h|r
    int type;
    char* param1_str = NULL;
    char* idS = ExtractKeyFromLink(text, spellKeys, &type, &param1_str);
    if (idS)
    {
        uint32 id;
        if (ExtractUInt32(&idS, id))
        {
            switch (type)
            {
                case SPELL_LINK_RAW:
                case SPELL_LINK_SPELL:
                case SPELL_LINK_ENCHANT:
                    return id;
                case SPELL_LINK_TALENT:
                {
                    // talent
                    TalentEntry const* talentEntry = sTalentStore.LookupEntry(id);
                    int32 rank;
                    if (talentEntry && ExtractInt32(&param1_str, rank))
                    {
                        if (rank < 0) // unlearned talent have in shift-link field -1 as rank
                        {
                            rank = 0;
                        }
                        if (rank < MAX_TALENT_RANK)
                        {
                            return talentEntry->RankID[rank];
                        }
                    }
                    break;
                }
            }
        }
    }

    // Name-based fallback
    char const* lookupName = idS ? idS : *text;
    if (!lookupName || !*lookupName)
    {
        return 0;
    }

    LocaleConstant locale = GetSessionDbcLocale();

    std::wstring wname;
    if (!Utf8toWStr(lookupName, wname))
    {
        return 0;
    }
    wstrToLower(wname);

    uint32 exactId = 0;
    std::vector<std::pair<uint32, std::string>> candidates;
    std::vector<std::pair<uint32, std::string>> fuzzyCandidates;

    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(id);
        if (!spell)
        {
            continue;
        }
        std::string sName = spell->SpellName[locale];

        if (spell->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL ||
            spell->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_LEARN_SPELL ||
            spell->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_LEARN_SPELL ||
            sName.empty() || sName.compare(0, 5, "Test ") == 0 ||
            sName.compare(0, 2, "ZZ") == 0 || sName.compare(0, 2, "zz") == 0)
        {
            continue;
        }

        std::wstring wSpellName;
        if (!Utf8toWStr(sName, wSpellName))
        {
            continue;
        }
        wstrToLower(wSpellName);

        if (wSpellName == wname)
        {
            if (exactId == 0)
            {
                exactId = id;
            }
            candidates.push_back({id, spell->SpellName[locale]});
        }
        else if (exactId == 0)
        {
            int loc = locale;
            if (!Utf8FitTo(sName, wname))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == locale)
                    {
                        continue;
                    }
                    sName = spell->SpellName[loc];
                    if (sName.empty())
                    {
                        continue;
                    }
                    if (Utf8FitTo(sName, wname))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                fuzzyCandidates.push_back({id, spell->SpellName[locale]});
            }
        }
    }

    if (!candidates.empty())
    {
        if (candidates.size() == 1)
        {
            return exactId;
        }

        SendSysMessage("Multiple spells found with that name:");
        for (auto const& c : candidates)
        {
            PSendSysMessage("  %u - %s", c.first, c.second.c_str());
        }
        SetSentErrorMessage(true);
        return 0;
    }

    if (fuzzyCandidates.empty())
    {
        SendSysMessage(LANG_COMMAND_NOSPELLFOUND);
        SetSentErrorMessage(true);
        return 0;
    }

    SendSysMessage("No exact match. Close matches:");
    for (auto const& c : fuzzyCandidates)
    {
        PSendSysMessage("  %u - %s", c.first, c.second.c_str());
    }
    SetSentErrorMessage(true);
    return 0;
}

/**
 * @brief Extracts a game teleport definition from a raw argument or teleport link.
 *
 * @param text The argument text pointer to parse.
 * @return GameTele const* The matching teleport definition, or NULL on failure.
 */
GameTele const* ChatHandler::ExtractGameTeleFromLink(char** text)
{
    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    char* cId = ExtractKeyFromLink(text, "Htele");
    if (!cId)
    {
        return NULL;
    }

    // id case (explicit or from shift link)
    uint32 id;
    if (ExtractUInt32(&cId, id))
    {
        return sObjectMgr.GetGameTele(id);
    }
    else
    {
        return sObjectMgr.GetGameTele(cId);
    }
}

enum GuidLinkType
{
    GUID_LINK_RAW        = -1,                              // non-link case
    GUID_LINK_PLAYER     = 0,
    GUID_LINK_CREATURE   = 1,
    GUID_LINK_GAMEOBJECT = 2,
};

static char const* const guidKeys[] =
{
        "Hplayer",
        "Hcreature",
        "Hgameobject",
    NULL
};

/**
 * @brief Extracts a player, creature, or game object GUID from a raw argument or link.
 *
 * @param text The argument text pointer to parse.
 * @return ObjectGuid The extracted GUID, or an empty GUID on failure.
 */
ObjectGuid ChatHandler::ExtractGuidFromLink(char** text)
{
    int type = 0;

    // |color|Hcreature:creature_guid|h[name]|h|r
    // |color|Hgameobject:go_guid|h[name]|h|r
    // |color|Hplayer:name|h[name]|h|r
    char* idS = ExtractKeyFromLink(text, guidKeys, &type);
    if (!idS)
    {
        return ObjectGuid();
    }

    switch (type)
    {
        case GUID_LINK_RAW:
        case GUID_LINK_PLAYER:
        {
            std::string name = idS;
            if (!normalizePlayerName(name))
            {
                return ObjectGuid();
            }

            if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
            {
                return player->GetObjectGuid();
            }

            return sObjectMgr.GetPlayerGuidByName(name);
        }
        case GUID_LINK_CREATURE:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return ObjectGuid();
            }

            if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
            {
                return data->GetObjectGuid(lowguid);
            }
            else
            {
                return ObjectGuid();
            }
        }
        case GUID_LINK_GAMEOBJECT:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return ObjectGuid();
            }

            if (GameObjectData const* data = sObjectMgr.GetGOData(lowguid))
            {
                return ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, lowguid);
            }
            else
            {
                return ObjectGuid();
            }
        }
    }

    // unknown type?
    return ObjectGuid();
}

enum LocationLinkType
{
    LOCATION_LINK_RAW               = -1,                   // non-link case
    LOCATION_LINK_PLAYER            = 0,
    LOCATION_LINK_TELE              = 1,
    LOCATION_LINK_TAXINODE          = 2,
    LOCATION_LINK_CREATURE          = 3,
    LOCATION_LINK_GAMEOBJECT        = 4,
    LOCATION_LINK_CREATURE_ENTRY    = 5,
    LOCATION_LINK_GAMEOBJECT_ENTRY  = 6,
    LOCATION_LINK_AREATRIGGER       = 7,
    LOCATION_LINK_AREATRIGGER_TARGET = 8,
};

static char const* const locationKeys[] =
{
        "Htele",
        "Htaxinode",
        "Hplayer",
        "Hcreature",
        "Hgameobject",
        "Hcreature_entry",
        "Hgameobject_entry",
        "Hareatrigger",
        "Hareatrigger_target",
    NULL
};

/**
 * @brief Extracts map coordinates from a supported location-like link or player reference.
 *
 * @param text The argument text pointer to parse.
 * @param mapid Receives the destination map id.
 * @param x Receives the destination X coordinate.
 * @param y Receives the destination Y coordinate.
 * @param z Receives the destination Z coordinate.
 * @return true if a location was extracted; otherwise false.
 */
bool ChatHandler::ExtractLocationFromLink(char** text, uint32& mapid, float& x, float& y, float& z)
{
    int type = 0;

    // |color|Hplayer:name|h[name]|h|r
    // |color|Htele:id|h[name]|h|r
    // |color|Htaxinode:id|h[name]|h|r
    // |color|Hcreature:creature_guid|h[name]|h|r
    // |color|Hgameobject:go_guid|h[name]|h|r
    // |color|Hcreature_entry:creature_id|h[name]|h|r
    // |color|Hgameobject_entry:go_id|h[name]|h|r
    // |color|Hareatrigger:id|h[name]|h|r
    // |color|Hareatrigger_target:id|h[name]|h|r
    char* idS = ExtractKeyFromLink(text, locationKeys, &type);
    if (!idS)
    {
        return false;
    }

    switch (type)
    {
        case LOCATION_LINK_RAW:
        case LOCATION_LINK_PLAYER:
        {
            std::string name = idS;
            if (!normalizePlayerName(name))
            {
                return false;
            }

            if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
            {
                mapid = player->GetMapId();
                x = player->GetPositionX();
                y = player->GetPositionY();
                z = player->GetPositionZ();
                return true;
            }

            if (ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name))
            {
                // to point where player stay (if loaded)
                float o;
                bool in_flight;
                return Player::LoadPositionFromDB(guid, mapid, x, y, z, o, in_flight);
            }

            return false;
        }
        case LOCATION_LINK_TELE:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            GameTele const* tele = sObjectMgr.GetGameTele(id);
            if (!tele)
            {
                return false;
            }
            mapid = tele->mapId;
            x = tele->position_x;
            y = tele->position_y;
            z = tele->position_z;
            return true;
        }
        case LOCATION_LINK_TAXINODE:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
            if (!node)
            {
                return false;
            }
            mapid = node->map_id;
            x = node->x;
            y = node->y;
            z = node->z;
            return true;
        }
        case LOCATION_LINK_CREATURE:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return false;
            }

            if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
            {
                mapid = data->mapid;
                x = data->posX;
                y = data->posY;
                z = data->posZ;
                return true;
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_GAMEOBJECT:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return false;
            }

            if (GameObjectData const* data = sObjectMgr.GetGOData(lowguid))
            {
                mapid = data->mapid;
                x = data->posX;
                y = data->posY;
                z = data->posZ;
                return true;
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_CREATURE_ENTRY:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (ObjectMgr::GetCreatureTemplate(id))
            {
                FindCreatureData worker(id, m_session ? m_session->GetPlayer() : NULL);

                sObjectMgr.DoCreatureData(worker);

                if (CreatureDataPair const* dataPair = worker.GetResult())
                {
                    mapid = dataPair->second.mapid;
                    x = dataPair->second.posX;
                    y = dataPair->second.posY;
                    z = dataPair->second.posZ;
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_GAMEOBJECT_ENTRY:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (ObjectMgr::GetGameObjectInfo(id))
            {
                FindGOData worker(id, m_session ? m_session->GetPlayer() : NULL);

                sObjectMgr.DoGOData(worker);

                if (GameObjectDataPair const* dataPair = worker.GetResult())
                {
                    mapid = dataPair->second.mapid;
                    x = dataPair->second.posX;
                    y = dataPair->second.posY;
                    z = dataPair->second.posZ;
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_AREATRIGGER:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
            if (!atEntry)
            {
                PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
                SetSentErrorMessage(true);
                return false;
            }

            mapid = atEntry->mapid;
            x = atEntry->x;
            y = atEntry->y;
            z = atEntry->z;
            return true;
        }
        case LOCATION_LINK_AREATRIGGER_TARGET:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (!sAreaTriggerStore.LookupEntry(id))
            {
                PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
                SetSentErrorMessage(true);
                return false;
            }

            AreaTrigger const* at = sObjectMgr.GetAreaTrigger(id);
            if (!at)
            {
                PSendSysMessage(LANG_AREATRIGER_NOT_HAS_TARGET, id);
                SetSentErrorMessage(true);
                return false;
            }

            mapid = at->target_mapId;
            x = at->target_X;
            y = at->target_Y;
            z = at->target_Z;
            return true;
        }
    }

    // unknown type?
    return false;
}

/**
 * @brief Extracts and normalizes a player name from a player link.
 *
 * @param text The argument text pointer to parse.
 * @return std::string The normalized player name, or an empty string on failure.
 */
std::string ChatHandler::ExtractPlayerNameFromLink(char** text)
{
    // |color|Hplayer:name|h[name]|h|r
    char* name_str = ExtractKeyFromLink(text, "Hplayer");
    if (!name_str)
    {
        return "";
    }

    std::string name = name_str;
    if (!normalizePlayerName(name))
    {
        return "";
    }

    return name;
}

/**
 * Function extract at least one from request player data (pointer/guid/name) from args name/shift-link or selected player if no args
 *
 * @param args        variable pointer to non parsed args string, updated at function call to new position (with skipped white spaces)
 *
 * @param player      optional arg   One from 3 optional args must be provided at least (or more).
 * @param player_guid optional arg   For function success only one from provided args need get result
 * @param player_name optional arg   But if early arg get value then all later args will have its (if requested)
 *                                   if player_guid requested and not found then name also will not found
 *                                   So at success can be returned 2 cases: (player/guid/name) or (guid/name)
 *
 * @return           true if extraction successful
 */
bool ChatHandler::ExtractPlayerTarget(char** args, Player** player /*= NULL*/, ObjectGuid* player_guid /*= NULL*/, std::string* player_name /*= NULL*/)
{
    if (*args &&**  args)
    {
        std::string name = ExtractPlayerNameFromLink(args);
        if (name.empty())
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        Player* pl = sObjectMgr.GetPlayer(name.c_str());

        // if allowed player pointer
        if (player)
        {
            *player = pl;
        }

        // if need guid value from DB (in name case for check player existence)
        ObjectGuid guid = !pl && (player_guid || player_name) ? sObjectMgr.GetPlayerGuidByName(name) : ObjectGuid();

        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
        {
            *player_guid = pl ? pl->GetObjectGuid() : guid;
        }

        if (player_name)
        {
            *player_name = pl || guid ? name : "";
        }
    }
    else
    {
        Player* pl = getSelectedPlayer();
        // if allowed player pointer
        if (player)
        {
            *player = pl;
        }

        ObjectGuid guid = pl ? pl->GetObjectGuid() : ObjectGuid();

        // Console with a selected player that is now offline: fall back to stored GUID
        if (!pl && !m_session)
        {
            uint32 accountId = GetAccountId();
            auto itr = m_consoleSelectedPlayers.find(accountId);
            if (itr != m_consoleSelectedPlayers.end())
            {
                guid = itr->second;
            }
        }

        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
        {
            *player_guid = guid;
        }

        if (player_name)
        {
            if (pl)
            {
                *player_name = pl->GetName();
            }
            else if (guid)
            {
                std::string name;
                if (!sObjectMgr.GetPlayerNameByGUID(guid, name) || name.empty())
                {
                    if (player_guid)
                    {
                        *player_guid = ObjectGuid();
                    }
                    *player_name = "";
                }
                else
                {
                    *player_name = name;
                }
            }
            else
            {
                *player_name = "";
            }
        }
    }

    // some from req. data must be provided (note: name is empty if player not exist)
    if ((!player || !*player) && (!player_guid || !*player_guid) && (!player_name || player_name->empty()))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

/**
 * @brief Extracts an account id from an argument or selected player fallback.
 *
 * @param args The argument text pointer to parse.
 * @param accountName Optional output for the resolved account name.
 * @param targetIfNullArg Optional output for the selected player when no argument is given.
 * @return uint32 The extracted account id, or 0 on failure.
 */
uint32 ChatHandler::ExtractAccountId(char** args, std::string* accountName /*= NULL*/, Player** targetIfNullArg /*= NULL*/)
{
    uint32 account_id = 0;

    ///- Get the account name from the command line
    char* account_str = ExtractLiteralArg(args);

    if (!account_str)
    {
        if (!targetIfNullArg)
        {
            return 0;
        }

        /// only target player different from self allowed (if targetPlayer!=NULL then not console)
        Player* targetPlayer = getSelectedPlayer();
        if (!targetPlayer)
        {
            return 0;
        }

        account_id = targetPlayer->GetSession()->GetAccountId();

        if (accountName)
        {
            sAccountMgr.GetName(account_id, *accountName);
        }

        if (targetIfNullArg)
        {
            *targetIfNullArg = targetPlayer;
        }

        return account_id;
    }

    std::string account_name;

    if (ExtractUInt32(&account_str, account_id))
    {
        if (!sAccountMgr.GetName(account_id, account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_str);
            SetSentErrorMessage(true);
            return 0;
        }
    }
    else
    {
        account_name = account_str;
        if (!Utf8ToUpperOnlyLatin(account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }

        account_id = sAccountMgr.GetId(account_name);
        if (!account_id)
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }
    }

    if (accountName)
    {
        *accountName = account_name;
    }

    if (targetIfNullArg)
    {
        *targetIfNullArg = NULL;
    }

    return account_id;
}

struct RaceMaskName
{
    char const* literal;
    uint32 raceMask;
};

static RaceMaskName const raceMaskNames[] =
{
    // races
    { "human", (1 << (RACE_HUMAN - 1))   },
    { "orc", (1 << (RACE_ORC - 1))     },
    { "dwarf", (1 << (RACE_DWARF - 1))   },
    { "nightelf", (1 << (RACE_NIGHTELF - 1))},
    { "undead", (1 << (RACE_UNDEAD - 1))  },
    { "tauren", (1 << (RACE_TAUREN - 1))  },
    { "gnome", (1 << (RACE_GNOME - 1))   },
    { "troll", (1 << (RACE_TROLL - 1))   },

    // masks
    { "alliance", RACEMASK_ALLIANCE },
    { "horde",    RACEMASK_HORDE },
    { "all", RACEMASK_ALL_PLAYABLE },

    // terminator
    { NULL, 0 }
};

/**
 * @brief Extracts a race mask from a numeric value or named preset.
 *
 * @param text The argument text pointer to parse.
 * @param raceMask Receives the resulting race mask.
 * @param maskName Optional output for the resolved preset name.
 * @return true if a race mask was extracted; otherwise false.
 */
bool ChatHandler::ExtractRaceMask(char** text, uint32& raceMask, char const** maskName /*=NULL*/)
{
    if (ExtractUInt32(text, raceMask))
    {
        if (maskName)
        {
            *maskName = "custom mask";
        }
    }
    else
    {
        for (RaceMaskName const* itr = raceMaskNames; itr->literal; ++itr)
        {
            if (ExtractLiteralArg(text, itr->literal))
            {
                raceMask = itr->raceMask;

                if (maskName)
                {
                    *maskName = itr->literal;
                }
                break;
            }
        }

        if (!raceMask)
        {
            return false;
        }
    }

    return true;
}
