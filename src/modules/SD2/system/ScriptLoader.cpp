/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013  ScriptDev2 <http://www.scriptdev2.com/>
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

#include "precompiled.h"

// eastern kingdoms
void AddSC_blackrock_depths();                       // blackrock_depths
void AddSC_boss_ambassador_flamelash();
void AddSC_boss_draganthaurissan();
void AddSC_boss_general_angerforge();
void AddSC_boss_high_interrogator_gerstahn();
void AddSC_instance_blackrock_depths();
void AddSC_boss_overlordwyrmthalak();                // blackrock_spire
void AddSC_boss_pyroguard_emberseer();
void AddSC_boss_gyth();
void AddSC_instance_blackrock_spire();
void AddSC_boss_razorgore();                         // blackwing_lair
void AddSC_boss_vaelastrasz();
void AddSC_boss_broodlord();
void AddSC_boss_firemaw();
void AddSC_boss_ebonroc();
void AddSC_boss_flamegor();
void AddSC_boss_chromaggus();
void AddSC_boss_nefarian();
void AddSC_boss_victor_nefarius();
void AddSC_instance_blackwing_lair();
void AddSC_boss_mr_smite();                          // deadmines
void AddSC_deadmines();
void AddSC_instance_deadmines();
void AddSC_gnomeregan();                             // gnomeregan
void AddSC_boss_thermaplugg();
void AddSC_instance_gnomeregan();
void AddSC_boss_lucifron();                          // molten_core
void AddSC_boss_magmadar();
void AddSC_boss_gehennas();
void AddSC_boss_garr();
void AddSC_boss_baron_geddon();
void AddSC_boss_shazzrah();
void AddSC_boss_golemagg();
void AddSC_boss_sulfuron();
void AddSC_boss_majordomo();
void AddSC_boss_ragnaros();
void AddSC_instance_molten_core();
void AddSC_molten_core();
void AddSC_boss_anubrekhan();                        // naxxramas
void AddSC_boss_four_horsemen();
void AddSC_boss_faerlina();
void AddSC_boss_gluth();
void AddSC_boss_gothik();
void AddSC_boss_grobbulus();
void AddSC_boss_kelthuzad();
void AddSC_boss_loatheb();
void AddSC_boss_maexxna();
void AddSC_boss_noth();
void AddSC_boss_heigan();
void AddSC_boss_patchwerk();
void AddSC_boss_razuvious();
void AddSC_boss_sapphiron();
void AddSC_boss_thaddius();
void AddSC_instance_naxxramas();
void AddSC_boss_arcanist_doan();                     // scarlet_monastery
void AddSC_boss_herod();
void AddSC_boss_mograine_and_whitemane();
void AddSC_instance_scarlet_monastery();
void AddSC_boss_darkmaster_gandling();               // scholomance
void AddSC_boss_jandicebarov();
void AddSC_instance_scholomance();
void AddSC_shadowfang_keep();                        // shadowfang_keep
void AddSC_instance_shadowfang_keep();
void AddSC_boss_maleki_the_pallid();                 // stratholme
void AddSC_boss_cannon_master_willey();
void AddSC_boss_baroness_anastari();
void AddSC_boss_dathrohan_balnazzar();
void AddSC_instance_stratholme();
void AddSC_stratholme();
void AddSC_instance_sunken_temple();                 // sunken_temple
void AddSC_sunken_temple();
void AddSC_boss_archaedas();                         // uldaman
void AddSC_instance_uldaman();
void AddSC_uldaman();
void AddSC_boss_arlokk();                            // zulgurub
void AddSC_boss_hakkar();
void AddSC_boss_hazzarah();
void AddSC_boss_jeklik();
void AddSC_boss_jindo();
void AddSC_boss_mandokir();
void AddSC_boss_marli();
void AddSC_boss_ouro();
void AddSC_boss_renataki();
void AddSC_boss_thekal();
void AddSC_boss_venoxis();
void AddSC_instance_zulgurub();

void AddSC_alterac_mountains();
void AddSC_arathi_highlands();
void AddSC_blasted_lands();
void AddSC_boss_kazzakAI();
void AddSC_burning_steppes();
void AddSC_dun_morogh();
void AddSC_eastern_plaguelands();
void AddSC_elwynn_forest();
void AddSC_hinterlands();
void AddSC_ironforge();
void AddSC_loch_modan();
void AddSC_redridge_mountains();
void AddSC_searing_gorge();
void AddSC_silverpine_forest();
void AddSC_stormwind_city();
void AddSC_stranglethorn_vale();
void AddSC_swamp_of_sorrows();
void AddSC_tirisfal_glades();
void AddSC_undercity();
void AddSC_western_plaguelands();
void AddSC_westfall();
void AddSC_wetlands();

void AddEasternKingdomsScripts()
{
    AddSC_blackrock_depths();                               // blackrock_depths
    AddSC_boss_ambassador_flamelash();
    AddSC_boss_draganthaurissan();
    AddSC_boss_general_angerforge();
    AddSC_boss_high_interrogator_gerstahn();
    AddSC_instance_blackrock_depths();
    AddSC_boss_overlordwyrmthalak();                        // blackrock_spire
    AddSC_boss_pyroguard_emberseer();
    AddSC_boss_gyth();
    AddSC_instance_blackrock_spire();
    AddSC_boss_razorgore();                                 // blackwing_lair
    AddSC_boss_vaelastrasz();
    AddSC_boss_broodlord();
    AddSC_boss_firemaw();
    AddSC_boss_ebonroc();
    AddSC_boss_flamegor();
    AddSC_boss_chromaggus();
    AddSC_boss_nefarian();
    AddSC_boss_victor_nefarius();
    AddSC_instance_blackwing_lair();
    AddSC_deadmines();                                      // deadmines
    AddSC_boss_mr_smite();
    AddSC_instance_deadmines();
    AddSC_gnomeregan();                                     // gnomeregan
    AddSC_boss_thermaplugg();
    AddSC_instance_gnomeregan();
    AddSC_boss_lucifron();                                  // molten_core
    AddSC_boss_magmadar();
    AddSC_boss_gehennas();
    AddSC_boss_garr();
    AddSC_boss_baron_geddon();
    AddSC_boss_shazzrah();
    AddSC_boss_golemagg();
    AddSC_boss_sulfuron();
    AddSC_boss_majordomo();
    AddSC_boss_ragnaros();
    AddSC_instance_molten_core();
    AddSC_molten_core();
    AddSC_boss_anubrekhan();                                // naxxramas
    AddSC_boss_four_horsemen();
    AddSC_boss_faerlina();
    AddSC_boss_gluth();
    AddSC_boss_gothik();
    AddSC_boss_grobbulus();
    AddSC_boss_kelthuzad();
    AddSC_boss_loatheb();
    AddSC_boss_maexxna();
    AddSC_boss_noth();
    AddSC_boss_heigan();
    AddSC_boss_patchwerk();
    AddSC_boss_razuvious();
    AddSC_boss_sapphiron();
    AddSC_boss_thaddius();
    AddSC_instance_naxxramas();
    AddSC_boss_arcanist_doan();                             // scarlet_monastery
    AddSC_boss_herod();
    AddSC_boss_mograine_and_whitemane();
    AddSC_instance_scarlet_monastery();
    AddSC_boss_darkmaster_gandling();                       // scholomance
    AddSC_boss_jandicebarov();
    AddSC_instance_scholomance();
    AddSC_shadowfang_keep();                                // shadowfang_keep
    AddSC_instance_shadowfang_keep();
    AddSC_boss_maleki_the_pallid();                         // stratholme
    AddSC_boss_cannon_master_willey();
    AddSC_boss_baroness_anastari();
    AddSC_boss_dathrohan_balnazzar();
    AddSC_instance_stratholme();
    AddSC_stratholme();
    AddSC_instance_sunken_temple();                         // sunken_temple
    AddSC_sunken_temple();
    AddSC_boss_archaedas();                                 // uldaman
    AddSC_instance_uldaman();
    AddSC_uldaman();
    AddSC_boss_arlokk();                                    // zulgurub
    AddSC_boss_hakkar();
    AddSC_boss_hazzarah();
    AddSC_boss_jeklik();
    AddSC_boss_jindo();
    AddSC_boss_mandokir();
    AddSC_boss_marli();
    AddSC_boss_ouro();
    AddSC_boss_renataki();
    AddSC_boss_thekal();
    AddSC_boss_venoxis();
    AddSC_instance_zulgurub();

    AddSC_alterac_mountains();
    AddSC_arathi_highlands();
    AddSC_blasted_lands();
    AddSC_boss_kazzakAI();
    AddSC_burning_steppes();
    AddSC_dun_morogh();
    AddSC_eastern_plaguelands();
    AddSC_elwynn_forest();
    AddSC_hinterlands();
    AddSC_ironforge();
    AddSC_loch_modan();
    AddSC_redridge_mountains();
    AddSC_searing_gorge();
    AddSC_silverpine_forest();
    AddSC_stormwind_city();
    AddSC_stranglethorn_vale();
    AddSC_swamp_of_sorrows();
    AddSC_tirisfal_glades();
    AddSC_undercity();
    AddSC_western_plaguelands();
    AddSC_westfall();
    AddSC_wetlands();
}

// kalimdor
void AddSC_instance_blackfathom_deeps();             // blackfathom_deeps
void AddSC_blackfathom_deeps();
void AddSC_dire_maul();                              // dire_maul
void AddSC_instance_dire_maul();
void AddSC_boss_noxxion();                           // maraudon
void AddSC_boss_onyxia();                            // onyxias_lair
void AddSC_instance_onyxias_lair();
void AddSC_npc_onyxian_warder();
void AddSC_instance_razorfen_downs();                // razorfen_downs
void AddSC_razorfen_downs();
void AddSC_instance_razorfen_kraul();                // razorfen_kraul
void AddSC_razorfen_kraul();
void AddSC_boss_ayamiss();                           // ruins_of_ahnqiraj
void AddSC_boss_buru();
void AddSC_boss_kurinnaxx();
void AddSC_boss_ossirian();
void AddSC_boss_moam();
void AddSC_boss_rajaxx();
void AddSC_ruins_of_ahnqiraj();
void AddSC_instance_ruins_of_ahnqiraj();
void AddSC_boss_cthun();                             // temple_of_ahnqiraj
void AddSC_boss_fankriss();
void AddSC_boss_huhuran();
void AddSC_bug_trio();
void AddSC_boss_sartura();
void AddSC_boss_skeram();
void AddSC_boss_twinemperors();
void AddSC_boss_viscidus();
void AddSC_mob_anubisath_sentinel();
void AddSC_instance_temple_of_ahnqiraj();
void AddSC_instance_wailing_caverns();               // wailing_caverns
void AddSC_wailing_caverns();
void AddSC_boss_zumrah();                            // zulfarrak
void AddSC_instance_zulfarrak();
void AddSC_zulfarrak();

void AddSC_ashenvale();
void AddSC_azshara();
void AddSC_boss_azuregos();
void AddSC_darkshore();
void AddSC_desolace();
void AddSC_durotar();
void AddSC_dustwallow_marsh();
void AddSC_felwood();
void AddSC_feralas();
void AddSC_moonglade();
void AddSC_mulgore();
void AddSC_orgrimmar();
void AddSC_silithus();
void AddSC_stonetalon_mountains();
void AddSC_tanaris();
void AddSC_teldrassil();
void AddSC_the_barrens();
void AddSC_thousand_needles();
void AddSC_thunder_bluff();
void AddSC_ungoro_crater();
void AddSC_winterspring();

void AddKalimdorScripts()
{
    AddSC_instance_blackfathom_deeps();                     // blackfathom deeps
    AddSC_blackfathom_deeps();
    AddSC_dire_maul();                                      // dire_maul
    AddSC_instance_dire_maul();
    AddSC_boss_noxxion();                                   // maraudon
    AddSC_boss_onyxia();                                    // onyxias_lair
    AddSC_instance_onyxias_lair();
    AddSC_npc_onyxian_warder();
    AddSC_instance_razorfen_downs();                        // razorfen_downs
    AddSC_razorfen_downs();
    AddSC_instance_razorfen_kraul();                        // razorfen_kraul
    AddSC_razorfen_kraul();
    AddSC_boss_ayamiss();                                   // ruins_of_ahnqiraj
    AddSC_boss_buru();
    AddSC_boss_kurinnaxx();
    AddSC_boss_ossirian();
    AddSC_boss_moam();
    AddSC_boss_rajaxx();
    AddSC_ruins_of_ahnqiraj();
    AddSC_instance_ruins_of_ahnqiraj();
    AddSC_boss_cthun();                                     // temple_of_ahnqiraj
    AddSC_boss_fankriss();
    AddSC_boss_huhuran();
    AddSC_bug_trio();
    AddSC_boss_sartura();
    AddSC_boss_skeram();
    AddSC_boss_twinemperors();
    AddSC_boss_viscidus();
    AddSC_mob_anubisath_sentinel();
    AddSC_instance_temple_of_ahnqiraj();
    AddSC_instance_wailing_caverns();                       // wailing_caverns
    AddSC_wailing_caverns();
    AddSC_boss_zumrah();                                    // zulfarrak
    AddSC_zulfarrak();
    AddSC_instance_zulfarrak();

    AddSC_ashenvale();
    AddSC_azshara();
    AddSC_boss_azuregos();
    AddSC_darkshore();
    AddSC_desolace();
    AddSC_durotar();
    AddSC_dustwallow_marsh();
    AddSC_felwood();
    AddSC_feralas();
    AddSC_moonglade();
    AddSC_mulgore();
    AddSC_orgrimmar();
    AddSC_silithus();
    AddSC_stonetalon_mountains();
    AddSC_tanaris();
    AddSC_teldrassil();
    AddSC_the_barrens();
    AddSC_thousand_needles();
    AddSC_thunder_bluff();
    AddSC_ungoro_crater();
    AddSC_winterspring();

}

// world
void AddSC_areatrigger_scripts();
void AddSC_bosses_emerald_dragons();
void AddSC_generic_creature();
void AddSC_go_scripts();
void AddSC_guards();
void AddSC_item_scripts();
void AddSC_npc_professions();
void AddSC_npcs_special();
void AddSC_spell_scripts();
void AddSC_world_map_scripts();

void AddWorldScripts()
{
    AddSC_areatrigger_scripts();
    AddSC_bosses_emerald_dragons();
    AddSC_generic_creature();
    AddSC_go_scripts();
    AddSC_guards();
    AddSC_item_scripts();
    AddSC_npc_professions();
    AddSC_npcs_special();
    AddSC_spell_scripts();
    AddSC_world_map_scripts();
}

// battlegrounds
void AddSC_battleground();

void AddBattlegroundScripts()
{
    AddSC_battleground();
}

// initialize scripts
void AddScripts()
{
    AddWorldScripts();
    AddEasternKingdomsScripts();
    AddKalimdorScripts();
    AddBattlegroundScripts();
}
