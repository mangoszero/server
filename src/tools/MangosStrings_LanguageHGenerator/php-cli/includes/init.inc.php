<?php
    echo "=== START OF SCRIPT ===\n";
    require_once "constants.php";

    // Determine which OS is running the script
    $CURRENT_OS = null;
    if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') 
    {
        $CURRENT_OS = OS_WINDOWS;
    } 
    else 
    {
        $CURRENT_OS = OS_LINUX;
    }

    echo "This script is running under a " . ($CURRENT_OS == OS_WINDOWS ? "WINDOWS" : "LINUX" ) ." operating system.". NEWLINE;
    
    require_once "config.inc.php";
    require_once "Common.Functions.php";
?>
