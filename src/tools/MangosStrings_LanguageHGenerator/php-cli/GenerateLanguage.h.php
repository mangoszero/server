<?php
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
 
 /* Generator Initiated by By @Elmsroth */

    require_once "includes/init.inc.php";
    
    echo "Which core version should the script generate the 'Language.h' file ?" .NEWLINE;
    
    displayChoices();
   
    $input = readline("Enter your choice : ");

    $config = getConfig($input);
    if($config == null)
    {
        echo "ERROR - Unknow selection : $input" .NEWLINE;
        goto end;
    }

    // Start MYSQL connection 
    $connStr = 'mysql:host='.DB_HOST.':'.DB_PORT.';dbname='.$config["DB"];
    $worldPDO = new PDO($connStr, DB_USER, DB_PASS);
    
    echo "> Generating 'Language.h' source file..." . NEWLINE;

    $sqlQueryText = "SELECT `entry`, `content_default`, `source_enum_tag` FROM `mangos_string` WHERE `source_enum_wrapper`='MangosStrings' ORDER By `entry` ASC;" ;
 
    $template = file_get_contents("templates/MangosZero_Language.h.tpl");

    if($template === false)
    {
        goto end;
    }
    
    $stm = $worldPDO->query($sqlQueryText);
    $rows = $stm->fetchAll(PDO::FETCH_ASSOC);

    $fileContent = "";

    $maxTagLength = 0;

    // find max tag Length
    foreach($rows as $row)
    {
        $tagLen = strlen($row["source_enum_tag"]);
        if($tagLen>$maxTagLength)
        {
            $maxTagLength = $tagLen;
        }
    }

    foreach($rows as $row)
    {
        $tagLen = strlen($row["source_enum_tag"]);
        $line = spaces(4)  . $row["source_enum_tag"] . spaces($maxTagLength - $tagLen + 2) . "= " .$row["entry"] ."," . spaces(4) . "/* " .$row["content_default"] ." */" . NEWLINE;
        $fileContent .= $line;
    }


    $output = str_replace ("{CONTENT}", $fileContent, $template);
 
    $outputDir = trim(OUTPUT_DIR);
    // If output dir is empty we will have to create output folder
    if($outputDir == "")
    {
        $outputDir = "../out/MangosZero/";
        if(folder_exist($outputDir) === false)
        {
            mkdir($outputDir, 0777, true);
        }
    }
    else
    {
        if($CURRENT_OS == OS_WINDOWS)
        {
            $outputDir = str_replace("\\", "/", $outputDir);
        }
    }

    // Check for missing trailing slash
    if( get_nth_char($outputDir, strlen($outputDir)) != "/")
    {
        $outputDir .= "/";
    }

    $outFile = $outputDir ."Language.h";

    file_put_contents($outFile,$output);

    echo "> OUT file = ".$outFile .NEWLINE;

    // Write the file


end:
 require_once "includes/finish.inc.php";
?>
