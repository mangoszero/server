#!/bin/bash

# This code is part of MaNGOS. Contributor & Copyright details are in AUTHORS/THANKS.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Expected param 1 to be 'a' for all, else ask some questions

## Normal log file (if not overwritten by second param)
LOG_FILE="MaNGOSExtractor.log"

## Detailed log file
DETAIL_LOG_FILE="MaNGOSExtractor_detailed.log"

## ! Use below only for finetuning or if you know what you are doing !
USE_AD="0"
USE_VMAPS="0"
USE_MMAPS="0"
USE_MMAPS_OFFMESH="0"
USE_MMAPS_DELAY=""

DisplayHeader()
{
    clear
    echo "  __  __      _  _  ___  ___  ___        "
    echo " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|    "
    echo " | |\\/| / _\` | .\` | (_ | (_) \\__ \\  "
    echo " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/   "
    echo "                                         "
    echo " For help and support please visit:      "
    echo " Website/Forum/Wiki: https://getmangos.eu"
    echo "========================================================="
}

if [ "$1" = "a" ]
then
    ## extract all
    USE_AD="1"
    USE_VMAPS="1"
    USE_MMAPS="1"
    USE_MMAPS_DELAY="no"
else
    ## do some questioning!
    DisplayHeader
    echo "  Welcome to the MaNGOS Data Extraction helper script !"
    echo "========================================================="
    echo
    echo "Should all data (dbc, maps, vmaps and mmaps) be extracted ?"
    read -p"(Selecting 'n' will give you the option to manually choose yes/no for each step) (y/n): " line

    if [ "$line" = "y" ]
    then
        ## extract all
        USE_AD="1"
        USE_VMAPS="1"
        USE_MMAPS="1"
    else
        DisplayHeader
        echo
        read -p"Should dbc and maps be extracted? (y/n): " line
        if [ "$line" = "y" ]; then USE_AD="1"; fi

        DisplayHeader
        echo
        read -p"Should vmaps be extracted? (y/n): " line
        if [ "$line" = "y" ]; then USE_VMAPS="1"; fi

        DisplayHeader
        echo
        echo "WARNING! Extracting mmaps will take several hours!"
        echo "(you can later tell the extractor to delay starting)"
        echo
        read -p"Should mmaps be extracted? (y/n): " line
        if [ "$line" = "y" ]; then
            USE_MMAPS="1";
        else
            echo
            read -p"Only reextract offmesh tiles for mmaps? (y/n): " line
            if [ "$line" = "y" ]; then USE_MMAPS_OFFMESH="1"; fi
        fi
    fi
fi

# Ensure bins are marked executable
for bin in ./map-extractor ./movemap-generator ./vmap-extractor; do
    if [ -f "$bin" ]; then
        echo "Marking $bin executable"
        chmod +x "$bin"
    fi
done

## Special case: Only reextract offmesh tiles
if [ "$USE_MMAPS_OFFMESH" = "1" ]; then
    echo "Only extracting offmesh meshes"
    sh ./MoveMapGen.sh "offmesh" $LOG_FILE $DETAIL_LOG_FILE
    exit 0
fi

## MMap Extraction specific
if [ "$USE_MMAPS" = "1" ]; then
    ## Obtain number of processes
    DisplayHeader
    echo
    read -p"How many CPUs should be used for extracting mmaps? (1-4): " line
    echo

    while [ "$line" -lt 1 -o "$line" -gt 4 ]; do
        echo "Only number between 1 and 4 are supported!"
        read -p"Please enter how many CPUs should be used for extracting mmaps? (1-4): " line
    done
    NUM_CPU=$line

    ## Extract MMaps delayed?
    DisplayHeader
    echo
    echo "MMap extraction can be set up with a delayed start."
    echo
    echo "If you *do not* want MMap Extraction to start delayed, just press return"
    echo
    echo "Otherwise enter a number followed by s for seconds, m for minutes, h for hours"
    echo "Example: \"3h\" - will start mmap extraction in 3 hours"
    echo
    read -p"MMap Extraction Delay (leave blank for direct extraction): "  line
    if [ "$line" != "" ]; then
        USE_MMAPS_DELAY=$line
    fi
fi

## Give some status
DisplayHeader
echo
echo "Current Extraction Settings: DBCs/maps: $USE_AD"
echo "                                 vmaps: $USE_VMAPS"
echo "                                 mmaps: $USE_MMAPS using $NUM_CPU processes"
if [ "$USE_MMAPS_DELAY" != "" ]; then
    echo
    echo "MMap Extraction will be delayed by: $USE_MMAPS_DELAY"
fi
echo
if [ "$1" != "a" ]; then
    echo "If you don't like these settings, interrupt with CTRL+C"
    echo
    read -p"Press any key to proceed" line
fi

echo "$(date): Start extracting data for MaNGOS" | tee $LOG_FILE

## Handle log messages
if [ "$USE_AD" = "1" ]; then
    echo "DBC and map files will be extracted" | tee -a $LOG_FILE
else
    echo "DBC and map files won't be extracted!" | tee -a $LOG_FILE
fi

if [ "$USE_VMAPS" = "1" ]; then
    echo "Vmaps will be extracted" | tee -a $LOG_FILE
else
    echo "Vmaps won't be extracted!" | tee -a $LOG_FILE
fi

if [ "$USE_MMAPS" = "1" ]; then
    echo "Mmaps will be extracted with $NUM_CPU processes" | tee -a $LOG_FILE
else
    echo "Mmaps files won't be extracted!" | tee -a $LOG_FILE
fi

echo | tee -a $LOG_FILE

echo "$(date): Start extracting data for MaNGOS, DBCs/maps $USE_AD, vmaps $USE_VMAPS, mmaps $USE_MMAPS on $NUM_CPU processes" | tee $DETAIL_LOG_FILE
echo | tee -a $DETAIL_LOG_FILE

## Extract dbcs and maps
if [ "$USE_AD" = "1" ]; then
    echo "$(date): Start extraction of DBCs and map files..." | tee -a $LOG_FILE
    ./map-extractor | tee -a $DETAIL_LOG_FILE
    echo "$(date): Extracting of DBCs and map files finished" | tee -a $LOG_FILE
    echo | tee -a $LOG_FILE
    echo | tee -a $DETAIL_LOG_FILE
fi

## Extract vmaps
if [ "$USE_VMAPS" = "1" ]; then
    echo "$(date): Start extraction of vmaps..." | tee -a $LOG_FILE
    ./vmap-extractor | tee -a $DETAIL_LOG_FILE
    echo "$(date): Extracting of vmaps finished" | tee -a $LOG_FILE

    echo | tee -a $LOG_FILE
    echo | tee -a $DETAIL_LOG_FILE
fi

## Extract mmaps
if [ "$USE_MMAPS" = "1" ]; then
    if [ "$USE_MMAPS_DELAY" != "" ]; then
        echo "Extracting of MMaps is set to be started delayed by $USE_MMAPS_DELAY"
        echo "Current time: $(date)"
        sleep $USE_MMAPS_DELAY
    fi
    sh ./MoveMapGen.sh $NUM_CPU $LOG_FILE $DETAIL_LOG_FILE
fi
