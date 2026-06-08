<?php

    function spaces($nbSpaces)
    {
        $str = "";
        for($i = 0; $i < $nbSpaces ; ++$i)
        {
            $str .= SPACE;
        }

        return $str;
    }

    function get_nth_char($string, $index) {
        return substr($string, strlen($string) - $index - 1, 1);
    }

    /**
     * Checks if a folder exist and return canonicalized absolute pathname (sort version)
     * @param string $folder the path being checked.
     * @return mixed returns the canonicalized absolute pathname on success otherwise FALSE is returned
     */
    function folder_exist($folder)
    {
        // Get canonicalized absolute pathname
        $path = realpath($folder);

        // If it exist, check if it's a directory
        return ($path !== false AND is_dir($path)) ? $path : false;
    }

    function displayChoices()
    {
        echo "\t[0] - Mangos Zero" .NEWLINE;
        echo "\t[1] - Mangos One" .NEWLINE;
        echo "\t[2] - Mangos Two" .NEWLINE;
        echo "\t[3] - Mangos Three" .NEWLINE;
        echo "\t[4] - Mangos Four" .NEWLINE;
    }

    function getConfig($input)
    {
        switch($input)
        {
            case 0 :
            case 1 :
            case 2 :
            case 3 :
            case 4 :
            {
                return SHARED_CONFIGS[$input];
            }
            default : 
            {
                return null;
            }
        }
    }

?>
