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

#include "Chat.h"
#include "Language.h"
#include "SpellMgr.h"

 /**********************************************************************
      CommandTable : learnCommandTable
 /***********************************************************************/

bool ChatHandler::HandleUnLearnCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    uint32 spell_id = ExtractSpellIdFromLink(&args);
    if (!spell_id)
    {
        return false;
    }

    bool allRanks = ExtractLiteralArg(&args, "all") != NULL;
    if (!allRanks && *args)                                 // can be fail also at syntax error
    {
        return false;
    }

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (allRanks)
    {
        spell_id = sSpellMgr.GetFirstSpellInChain(spell_id);
    }

    if (target->HasSpell(spell_id))
    {
        target->removeSpell(spell_id, false, !allRanks);
    }
    else
    {
        SendSysMessage(LANG_FORGET_SPELL);
    }

    return true;
}

bool ChatHandler::HandleLearnAllCommand(char* /*args*/)
{
    static const char* allSpellList[] =
    {
        "3365",
        "6233",
        "6247",
        "6246",
        "6477",
        "6478",
        "22810",
        "8386",
        "21651",
        "21652",
        "522",
        "7266",
        "8597",
        "2479",
        "22027",
        "6603",
        "5019",
        "133",
        "168",
        "227",
        "5009",
        "9078",
        "668",
        "203",
        "20599",
        "20600",
        "81",
        "20597",
        "20598",
        "20864",
        "1459",
        "5504",
        "587",
        "5143",
        "118",
        "5505",
        "597",
        "604",
        "1449",
        "1460",
        "2855",
        "1008",
        "475",
        "5506",
        "1463",
        "12824",
        "8437",
        "990",
        "5145",
        "8450",
        "1461",
        "759",
        "8494",
        "8455",
        "8438",
        "6127",
        "8416",
        "6129",
        "8451",
        "8495",
        "8439",
        "3552",
        "8417",
        "10138",
        "12825",
        "10169",
        "10156",
        "10144",
        "10191",
        "10201",
        "10211",
        "10053",
        "10173",
        "10139",
        "10145",
        "10192",
        "10170",
        "10202",
        "10054",
        "10174",
        "10193",
        "12826",
        "2136",
        "143",
        "145",
        "2137",
        "2120",
        "3140",
        "543",
        "2138",
        "2948",
        "8400",
        "2121",
        "8444",
        "8412",
        "8457",
        "8401",
        "8422",
        "8445",
        "8402",
        "8413",
        "8458",
        "8423",
        "8446",
        "10148",
        "10197",
        "10205",
        "10149",
        "10215",
        "10223",
        "10206",
        "10199",
        "10150",
        "10216",
        "10207",
        "10225",
        "10151",
        "116",
        "205",
        "7300",
        "122",
        "837",
        "10",
        "7301",
        "7322",
        "6143",
        "120",
        "865",
        "8406",
        "6141",
        "7302",
        "8461",
        "8407",
        "8492",
        "8427",
        "8408",
        "6131",
        "7320",
        "10159",
        "8462",
        "10185",
        "10179",
        "10160",
        "10180",
        "10219",
        "10186",
        "10177",
        "10230",
        "10181",
        "10161",
        "10187",
        "10220",
        "2018",
        "2663",
        "12260",
        "2660",
        "3115",
        "3326",
        "2665",
        "3116",
        "2738",
        "3293",
        "2661",
        "3319",
        "2662",
        "9983",
        "8880",
        "2737",
        "2739",
        "7408",
        "3320",
        "2666",
        "3323",
        "3324",
        "3294",
        "22723",
        "23219",
        "23220",
        "23221",
        "23228",
        "23338",
        "10788",
        "10790",
        "5611",
        "5016",
        "5609",
        "2060",
        "10963",
        "10964",
        "10965",
        "22593",
        "22594",
        "596",
        "996",
        "499",
        "768",
        "17002",
        "1448",
        "1082",
        "16979",
        "1079",
        "5215",
        "20484",
        "5221",
        "15590",
        "17007",
        "6795",
        "6807",
        "5487",
        "1446",
        "1066",
        "5421",
        "3139",
        "779",
        "6811",
        "6808",
        "1445",
        "5216",
        "1737",
        "5222",
        "5217",
        "1432",
        "6812",
        "9492",
        "5210",
        "3030",
        "1441",
        "783",
        "6801",
        "20739",
        "8944",
        "9491",
        "22569",
        "5226",
        "6786",
        "1433",
        "8973",
        "1828",
        "9495",
        "9006",
        "6794",
        "8993",
        "5203",
        "16914",
        "6784",
        "9635",
        "22830",
        "20722",
        "9748",
        "6790",
        "9753",
        "9493",
        "9752",
        "9831",
        "9825",
        "9822",
        "5204",
        "5401",
        "22831",
        "6793",
        "9845",
        "17401",
        "9882",
        "9868",
        "20749",
        "9893",
        "9899",
        "9895",
        "9832",
        "9902",
        "9909",
        "22832",
        "9828",
        "9851",
        "9883",
        "9869",
        "17406",
        "17402",
        "9914",
        "20750",
        "9897",
        "9848",
        "3127",
        "107",
        "204",
        "9116",
        "2457",
        "78",
        "18848",
        "331",
        "403",
        "2098",
        "1752",
        "11278",
        "11288",
        "11284",
        "6461",
        "2344",
        "2345",
        "6463",
        "2346",
        "2352",
        "775",
        "1434",
        "1612",
        "71",
        "2468",
        "2458",
        "2467",
        "7164",
        "7178",
        "7367",
        "7376",
        "7381",
        "21156",
        "5209",
        "3029",
        "5201",
        "9849",
        "9850",
        "20719",
        "22568",
        "22827",
        "22828",
        "22829",
        "6809",
        "8972",
        "9005",
        "9823",
        "9827",
        "6783",
        "9913",
        "6785",
        "6787",
        "9866",
        "9867",
        "9894",
        "9896",
        "6800",
        "8992",
        "9829",
        "9830",
        "780",
        "769",
        "6749",
        "6750",
        "9755",
        "9754",
        "9908",
        "20745",
        "20742",
        "20747",
        "20748",
        "9746",
        "9745",
        "9880",
        "9881",
        "5391",
        "842",
        "3025",
        "3031",
        "3287",
        "3329",
        "1945",
        "3559",
        "4933",
        "4934",
        "4935",
        "4936",
        "5142",
        "5390",
        "5392",
        "5404",
        "5420",
        "6405",
        "7293",
        "7965",
        "8041",
        "8153",
        "9033",
        "9034",
        //"9036", problems with ghost state
        "16421",
        "21653",
        "22660",
        "5225",
        "9846",
        "2426",
        "5916",
        "6634",
        //"6718", phasing stealth, annoying for learn all case.
        "6719",
        "8822",
        "9591",
        "9590",
        "10032",
        "17746",
        "17747",
        "8203",
        "11392",
        "12495",
        "16380",
        "23452",
        "4079",
        "4996",
        "4997",
        "4998",
        "4999",
        "5000",
        "6348",
        "6349",
        "6481",
        "6482",
        "6483",
        "6484",
        "11362",
        "11410",
        "11409",
        "12510",
        "12509",
        "12885",
        "13142",
        "21463",
        "23460",
        "11421",
        "11416",
        "11418",
        "1851",
        "10059",
        "11423",
        "11417",
        "11422",
        "11419",
        "11424",
        "11420",
        "27",
        "31",
        "33",
        "34",
        "35",
        "15125",
        "21127",
        "22950",
        "1180",
        "201",
        "12593",
        "12842",
        "16770",
        "6057",
        "12051",
        "18468",
        "12606",
        "12605",
        "18466",
        "12502",
        "12043",
        "15060",
        "12042",
        "12341",
        "12848",
        "12344",
        "12353",
        "18460",
        "11366",
        "12350",
        "12352",
        "13043",
        "11368",
        "11113",
        "12400",
        "11129",
        "16766",
        "12573",
        "15053",
        "12580",
        "12475",
        "12472",
        "12953",
        "12488",
        "11189",
        "12985",
        "12519",
        "16758",
        "11958",
        "12490",
        "11426",
        "3565",
        "3562",
        "18960",
        "3567",
        "3561",
        "3566",
        "3563",
        "1953",
        "2139",
        "12505",
        "13018",
        "12522",
        "12523",
        "5146",
        "5144",
        "5148",
        "8419",
        "8418",
        "10213",
        "10212",
        "10157",
        "12524",
        "13019",
        "12525",
        "13020",
        "12526",
        "13021",
        "18809",
        "13031",
        "13032",
        "13033",
        "4036",
        "3920",
        "3919",
        "3918",
        "7430",
        "3922",
        "3923",
        "7411",
        "7418",
        "7421",
        "13262",
        "7412",
        "7415",
        "7413",
        "7416",
        "13920",
        "13921",
        "7745",
        "7779",
        "7428",
        "7457",
        "7857",
        "7748",
        "7426",
        "13421",
        "7454",
        "13378",
        "7788",
        "14807",
        "14293",
        "7795",
        "6296",
        "20608",
        "755",
        "444",
        "427",
        "428",
        "442",
        "447",
        "3578",
        "3581",
        "19027",
        "3580",
        "665",
        "3579",
        "3577",
        "6755",
        "3576",
        "2575",
        "2577",
        "2578",
        "2579",
        "2580",
        "2656",
        "2657",
        "2576",
        "3564",
        "10248",
        "8388",
        "2659",
        "14891",
        "3308",
        "3307",
        "10097",
        "2658",
        "3569",
        "16153",
        "3304",
        "10098",
        "4037",
        "3929",
        "3931",
        "3926",
        "3924",
        "3930",
        "3977",
        "3925",
        "136",
        "228",
        "5487",
        "43",
        "202",
        "0"
    };

    int loop = 0;
    while (strcmp(allSpellList[loop], "0"))
    {
        uint32 spell = atol((char*)allSpellList[loop++]);

        if (m_session->GetPlayer()->HasSpell(spell))
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
        {
            PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
            continue;
        }

        m_session->GetPlayer()->learnSpell(spell, false);
    }

    SendSysMessage(LANG_COMMAND_LEARN_MANY_SPELLS);

    return true;
}

bool ChatHandler::HandleLearnAllGMCommand(char* /*args*/)
{
    static const char* gmSpellList[] =
    {
        "24347",                                            // Become A Fish, No Breath Bar
        "35132",                                            // Visual Boom
        "38488",                                            // Attack 4000-8000 AOE
        "38795",                                            // Attack 2000 AOE + Slow Down 90%
        "15712",                                            // Attack 200
        "1852",                                             // GM Spell Silence
        "31899",                                            // Kill
        "31924",                                            // Kill
        "29878",                                            // Kill My Self
        "26644",                                            // More Kill

        "28550",                                            // Invisible 24
        "23452",                                            // Invisible + Target
        "0"
    };

    uint16 gmSpellIter = 0;
    while (strcmp(gmSpellList[gmSpellIter], "0"))
    {
        uint32 spell = atol((char*)gmSpellList[gmSpellIter++]);

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
        {
            PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
            continue;
        }

        m_session->GetPlayer()->learnSpell(spell, false);
    }

    SendSysMessage(LANG_LEARNING_GM_SKILLS);
    return true;
}

bool ChatHandler::HandleLearnAllMyClassCommand(char* /*args*/)
{
    HandleLearnAllMySpellsCommand((char*)"");
    HandleLearnAllMyTalentsCommand((char*)"");
    return true;
}

bool ChatHandler::HandleLearnAllMySpellsCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();
    ChrClassesEntry const* clsEntry = sChrClassesStore.LookupEntry(player->getClass());
    if (!clsEntry)
    {
        return true;
    }
    uint32 family = clsEntry->spellfamily;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
        if (!entry)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry->spellId);
        if (!spellInfo)
        {
            continue;
        }

        // skip server-side/triggered spells
        if (spellInfo->spellLevel == 0)
        {
            continue;
        }

        // skip wrong class/race skills
        if (!player->IsSpellFitByClassAndRace(spellInfo->Id))
        {
            continue;
        }

        // skip other spell families
        if (spellInfo->SpellFamilyName != family)
        {
            continue;
        }

        // skip spells with first rank learned as talent (and all talents then also)
        uint32 first_rank = sSpellMgr.GetFirstSpellInChain(spellInfo->Id);
        if (GetTalentSpellCost(first_rank) > 0)
        {
            continue;
        }

        // skip broken spells
        if (!SpellMgr::IsSpellValid(spellInfo, player, false))
        {
            continue;
        }

        player->learnSpell(spellInfo->Id, false);
    }

    SendSysMessage(LANG_COMMAND_LEARN_CLASS_SPELLS);
    return true;
}

bool ChatHandler::HandleLearnAllMyTalentsCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();
    uint32 classMask = player->getClassMask();

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
        {
            continue;
        }

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTabInfo)
        {
            continue;
        }

        if ((classMask & talentTabInfo->ClassMask) == 0)
        {
            continue;
        }

        // search highest talent rank
        uint32 spellid = 0;

        for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] != 0)
            {
                spellid = talentInfo->RankID[rank];
                break;
            }
        }

        if (!spellid)                                       // ??? none spells in talent
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, player, false))
        {
            continue;
        }

        // learn highest rank of talent and learn all non-talent spell ranks (recursive by tree)
        player->learnSpellHighRank(spellid);
    }

    SendSysMessage(LANG_COMMAND_LEARN_CLASS_TALENTS);
    return true;
}

bool ChatHandler::HandleLearnAllLangCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    // skipping UNIVERSAL language (0)
    for (int i = 1; i < LANGUAGES_COUNT; ++i)
    {
        player->learnSpell(lang_description[i].spell_id, false);
    }

    SendSysMessage(LANG_COMMAND_LEARN_ALL_LANG);
    return true;
}

bool ChatHandler::HandleLearnAllDefaultCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    target->learnDefaultSpells();
    target->learnQuestRewardedSpells();

    PSendSysMessage(LANG_COMMAND_LEARN_ALL_DEFAULT_AND_QUEST, GetNameLink(target).c_str());
    return true;
}

bool ChatHandler::HandleLearnCommand(char* args)
{
    Player* player = m_session->GetPlayer();
    Player* targetPlayer = getSelectedPlayer();

    if (!targetPlayer)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
    {
        return false;
    }

    bool allRanks = ExtractLiteralArg(&args, "all") != NULL;
    if (!allRanks && *args)                                 // can be fail also at syntax error
    {
        return false;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, player))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    if (!allRanks && targetPlayer->HasSpell(spell))
    {
        if (targetPlayer == player)
        {
            SendSysMessage(LANG_YOU_KNOWN_SPELL);
        }
        else
        {
            PSendSysMessage(LANG_TARGET_KNOWN_SPELL, targetPlayer->GetName());
        }
        SetSentErrorMessage(true);
        return false;
    }

    if (allRanks)
    {
        targetPlayer->learnSpellHighRank(spell);
    }
    else
    {
        targetPlayer->learnSpell(spell, false);
    }

    return true;
}

void ChatHandler::HandleLearnSkillRecipesHelper(Player* player, uint32 skill_id)
{
    uint32 classmask = player->getClassMask();

    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(j);
        if (!skillLine)
        {
            continue;
        }

        // wrong skill
        if (skillLine->skillId != skill_id)
        {
            continue;
        }

        // not high rank
        if (skillLine->forward_spellid)
        {
            continue;
        }

        // skip racial skills
        if (skillLine->racemask != 0)
        {
            continue;
        }

        // skip wrong class skills
        if (skillLine->classmask && (skillLine->classmask & classmask) == 0)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->spellId);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, player, false))
        {
            continue;
        }

        player->learnSpell(skillLine->spellId, false);
    }
}

bool ChatHandler::HandleLearnAllCraftsCommand(char* /*args*/)
{
    for (uint32 i = 0; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(i);
        if (!skillInfo)
        {
            continue;
        }

        if (skillInfo->categoryId == SKILL_CATEGORY_PROFESSION || skillInfo->categoryId == SKILL_CATEGORY_SECONDARY)
        {
            // Learn only the crafts that actually are crafts (MaNGOS ZERO)
            if (skillInfo->id == SKILL_ENGINEERING || skillInfo->id == SKILL_BLACKSMITHING ||
                skillInfo->id == SKILL_LEATHERWORKING || skillInfo->id == SKILL_ALCHEMY ||
                skillInfo->id == SKILL_HERBALISM || skillInfo->id == SKILL_MINING ||
                skillInfo->id == SKILL_TAILORING || skillInfo->id == SKILL_ENCHANTING ||
                skillInfo->id == SKILL_SKINNING || skillInfo->id == SKILL_FIRST_AID ||
                skillInfo->id == SKILL_COOKING || skillInfo->id == SKILL_FISHING)
            {
                HandleLearnSkillRecipesHelper(m_session->GetPlayer(), skillInfo->id);
            }
        }
    }

    SendSysMessage(LANG_COMMAND_LEARN_ALL_CRAFT);
    return true;
}

bool ChatHandler::HandleLearnAllRecipesCommand(char* args)
{
    //  Learns all recipes of specified profession and sets skill to max
    //  Example: .learn all_recipes enchanting

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        return false;
    }

    if (!*args)
    {
        return false;
    }

    std::wstring wnamepart;

    if (!Utf8toWStr(args, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    std::string name;

    SkillLineEntry const* targetSkillInfo = NULL;
    for (uint32 i = 1; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(i);
        if (!skillInfo)
        {
            continue;
        }

        if (skillInfo->categoryId != SKILL_CATEGORY_PROFESSION &&
            skillInfo->categoryId != SKILL_CATEGORY_SECONDARY)
        {
            continue;
        }

        int loc = GetSessionDbcLocale();
        name = skillInfo->name[loc];
        if (name.empty())
        {
            continue;
        }

        if (!Utf8FitTo(name, wnamepart))
        {
            loc = 0;
            for (; loc < MAX_LOCALE; ++loc)
            {
                if (loc == GetSessionDbcLocale())
                {
                    continue;
                }

                name = skillInfo->name[loc];
                if (name.empty())
                {
                    continue;
                }

                if (Utf8FitTo(name, wnamepart))
                {
                    break;
                }
            }
        }

        if (loc < MAX_LOCALE)
        {
            targetSkillInfo = skillInfo;
            break;
        }
    }

    if (!targetSkillInfo)
    {
        return false;
    }

    HandleLearnSkillRecipesHelper(target, targetSkillInfo->id);

    uint16 maxLevel = target->GetPureMaxSkillValue(targetSkillInfo->id);
    target->SetSkill(targetSkillInfo->id, maxLevel, maxLevel);
    PSendSysMessage(LANG_COMMAND_LEARN_ALL_RECIPES, name.c_str());
    return true;
}
