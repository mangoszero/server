#!/bin/bash
###############################################################################
# MaNGOS Build Automation Script                                              #
# Written By: Ryan Ashley                                                     #
# Copyright (c) 2014-2015 MaNGOS Project                                      #
# https://getmangos.eu/                                                       #
#                                                                             #
# This program is free software; you can redistribute it and/or modify        #
# it under the terms of the GNU General Public License as published by        #
# the Free Software Foundation; either version 2 of the License, or           #
# (at your option) any later version.                                         #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                #
# GNU General Public License for more details.                                #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program; if not, write to the Free Software                 #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA     #
###############################################################################

# Global variables
DLGAPP="whiptail"
VERSION="0"
SRCPATH="$HOME/mangos/src"
INSTPATH="$HOME/mangos"
DB_PREFIX="zero"
P_DEBUG="0"
P_STD_MALLOC="1"
P_ACE_EXTERNAL="1"
P_PGRESQL="0"
P_TOOLS="0"
P_SD2="1"
P_ELUNA="1"



# Function to test for dialog
function UseDialog()
{
  # Search for dialog
  which dialog

  # See if dialog was found
  if [ $? -eq 0 ]; then
    DLGAPP="dialog"
  fi
}



# Function to detect the repos
function DetectLocalRepo()
{
  local CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

  # First see if the Windows stuff is in place
  if [ ! -d ../win ]; then
    Log "Windows files directory does not exist, assuming repo has not been cloned." 0
    return 0
  fi

  # See if the sources dircetory exists
  if [ ! -d ../src ]; then
    Log "Source files directory does not exist, assuming repo has not been cloned." 0
    return 0
  fi

  # Check for the CMake directory
  if [ ! -d ../cmake ]; then
    Log "CMake directory does not exist, assuming repo has not been cloned." 0
    return 0
  fi

  # Set the default paths based on the current location
  SRCPATH=$( dirname $CUR_DIR )
  SRCPATH=$( dirname $SRCPATH )

  # Log the detected path
  Log "Detected cloned repository in $SRCPATH" 0
}



# Function to log results
function Log()
{
  local TIMESTAMP=$( date +%Y-%m-%d:%H:%M:%S )

  # Check the number of parameters
  if [ $# -ne 2 ]; then
    echo "Logging usage: Log <message> <echo flag>"
    return 1
  fi

  # Echo to the console if requested
  if [ $2 -eq 1 ]; then
    echo "$1"
  fi

  # Append the string to the log
  echo "$TIMESTAMP $1" >> ~/getmangos.log
}



# Function to install prerequisite libraries
function GetPrerequisites()
{
  local OS_VER=0

  # Ask the user to continue
  $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Install Required Dependencies" \
    --yesno "Would you like to install the required build and development packages?" 8 60

  # Check the suer's response
  if [ $? -ne 0 ]; then
    Log "User declined to install required tools and development libraries." 1
    return 0
  fi

  # Handle Debian OS
  if [ -f "/etc/debian_version" ]; then
    # Inform the user of the need for root access
    $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Install Required Dependencies" \
      --yesno "Installing packages requires root access, which you will be prompted for.\nDo you want to proceed?" 8 60

    # Check the user's response
    if [ $? -ne 0 ]; then
      Log "User declined to proved root access for package installation." 1
      return 0
    fi

    # Grab the version of Debian installed on this system
    OS_VER=`cat /etc/debian_version`

    # Check for a valid version
    if [ $(echo "$OS_VER < 6.0" | bc) -eq 1 ] || [ $(echo "$OS_VER >= 8.0" | bc) -eq 1 ]; then
      Log "Error: Only Debian Squeeze and Wheezy are supported." 1
      return 1
    fi

    # Handle Debian Wheezy
    if [ $(echo "$OS_VER >= 7.0" | bc) -eq 1 ] && [ $(echo "$OS_VER < 8.0" | bc) -eq 1 ]; then
      # Install the prerequisite packages
      su -c "aptitude -y install build-essential linux-headers-$(uname -r) autoconf automake cmake libbz2-dev libace-dev libace-6.0.3 libssl-dev libmysqlclient-dev zlib1g-dev" root
    fi

    # Handle Debian Squeeze
    if [ $(echo "$OS_VER >= 6.0" | bc) -eq 1 ] && [ $(echo "$OS_VER < 7.0" | bc) -eq 1 ]; then
      # Install the prerequisite packages
      su -c "aptitude -y install build-essential linux-headers-$(uname -r) autoconf automake cmake libbz2-dev libace-dev libace-5.7.7 libssl-dev libmysqlclient-dev zlib1g-dev" root
    fi
  fi

  # See if a supported OS was detected
  if [ OS_VER -ne 0 ]; then
    # Log success
    Log "The development tools and libraries have been installed!" 1
  else
    # Note the error
    Log "Could not identify the current OS. Nothing was installed." 1
  fi
}



# Function to get the WoW version
function GetRelease()
{
  VERSION=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Choose WoW Release" \
    --menu "Select a version of WoW" 0 0 5 \
    0 "Original Release (Vanilla)" \
    1 "The Burning Crusade" \
    2 "Wrath of The Lich King" \
    3 "Cataclysm" \
    4 "Mists of Pandaria" \
    3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "Version selection cancelled by user. No changes have been made to your system." 1
    exit 0
  fi

  # Set some defaults based on the release
  case "$VERSION" in
    0)
      SRCPATH="$HOME/zero/src"
      INSTPATH="$HOME/zero"
      DB_PREFIX="zero"
      ;;

    1)
      SRCPATH="$HOME/one/src"
      INSTPATH="$HOME/one"
      DB_PREFIX="one"
      ;;

    2)
      SRCPATH="$HOME/two/src"
      INSTPATH="$HOME/two"
      DB_PREFIX="two"
      ;;

    3)
      SRCPATH="$HOME/three/src"
      INSTPATH="$HOME/three"
      DB_PREFIX="three"
      ;;

    4)
      SRCPATH="$HOME/four/src"
      INSTPATH="$HOME/four"
      DB_PREFIX="four"
      ;;

    *)
      Log "Error: Unknown version selected!" 1
      exit 1
      ;;
  esac

  # Now set the correct source path if the repo has been cloned already
  DetectLocalRepo
}



# Function to get the source and installation paths
function GetPaths()
{
  local TMPPATH="$HOME"

  # Set the source path
  TMPPATH=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Source-Code Path" \
    --inputbox "Default: $SRCPATH" 8 60 3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "Source path selection was cancelled. No changes have been made to your system." 1
    exit 0
  fi

  # Change the path only if it was modified
  if [ ! -z "$TMPPATH" ]; then
    SRCPATH="$TMPPATH"
  fi

  # Validate source path
  if [ ! -d "$SRCPATH" ]; then
    $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Path does not exist" \
      --yesno "Would you like to create the directory \"$SRCPATH\"?" 8 60

    if [ $? -eq 0 ]; then
      Log "Creating source path: $SRCPATH" 1
      mkdir -p "$SRCPATH" > /dev/null 2>&1

      # Check to see if the directory was created
      if [ $? -ne 0 ]; then
        Log "Error: Failed to create the specified source-code directory!" 1
        exit 1
      fi
    else
      Log "Source path creation cancelled. No modifications have been made to your system." 1
      exit 0
    fi
  else
    # Check for old sources
    if [ -d "$SRCPATH/server" ] || [ -d "$SRCPATH/database" ]; then
      # Ask to remove the old sources
      $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Path already exists" \
        --yesno "Would you like to remove the old sources? (Answer yes if you are cloning MaNGOS)" 9 60

      # Remove the old sources if requested
      if [ $? -eq 0 ]; then
        Log "Removing old sources from: $SRCPATH/*" 1
        rm -rf $SRCPATH/*

        # Check for removal failure
        if [ $? -ne 0 ]; then
          Log "Error: Failed to remove old sources!" 1
          exit 1
        fi
      fi
    fi
  fi

  # Set the installation path
  TMPPATH=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Installation Path" \
    --inputbox "Default: $INSTPATH" 8 60 3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "Install path selection cancelled. Only the source path has been created." 1
    exit 0
  fi

  # Change the path only if it was modified
  if [ ! -z "$TMPPATH" ]; then
    INSTPATH="$TMPPATH"
  fi

  # Validate install path
  if [ ! -d "$INSTPATH" ]; then
    $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Path does not exist" \
      --yesno "Would you like to create the directory \"$INSTPATH\"?" 8 60

    if [ $? -eq 0 ];then
      Log "Creating install path: $INSTPATH" 1
      mkdir -p "$INSTPATH" > /dev/null 2>&1

      # Check to see if the directory was created
      if [ $? -ne 0 ]; then
        Log "Error: Failed to create the specified installation directory!" 1
        exit 1
      fi
    else
      Log "Install path creation cancelled. Only the source path has been created."
      exit 0
    fi
  else
    # Check for an old installation
    if [ -d "$INSTPATH/bin" ] || [ -d "$INSTPATH/lib" ] || [ -d "$INSTPATH/include" ]; then
 
      # Ask to remove the old installation
      $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Path already exists" \
        --yesno "Would you like to uninstall the current version of MaNGOS first?" 0 0

      # Check the user's response
      if [ $? -eq 0 ]; then
        Log "Removing old MaNGOS installation..." 1

        # Clean up the binaries
        if [ -d "$INSTPATH/bin" ]; then
          rm -rf $INSTPATH/bin
        fi

        # Clean up the old includes
        if [ -d "$INSTPATH/include" ]; then
          rm -rf $INSTPATH/include
        fi

        # Clean up the library files
        if [ -d "$INSTPATH/lib" ]; then
          rm -rf $INSTPATH/lib
        fi

        # Clean up the old logs
        if [ -d "$INSTPATH/logs" ]; then
          rm -rf $INSTPATH/logs/*
        fi
      fi
    fi
  fi

  # Log the settings
  Log "Install path: $INSTPATH" 0
  Log "Source path: $SRCPATH" 0
}



# Function to clone or update sources
function GetMangos()
{
  local CLONE="0"
  local BRANCH="master"

  CLONE=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Clone or update MaNGOS" \
    --menu "Would you like to clone, update, or continue?" 0 0 3 \
    0 "Clone a fresh copy of MaNGOS" \
    1 "Update your existing copy of MaNGOS" \
    2 "Use existing copy" \
    3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "Source cloning cancelled. Only the install and source paths have been created." 1
    exit 0
  fi

  # Clone from scratch if selected
  if [[ $CLONE = *0* ]]; then
    # Pull a different branch?
    BRANCH=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Select Branch" \
      --inputbox "Default: master" 8 60 3>&2 2>&1 1>&3)

    # Exit if cancelled
    if [ $? -ne 0 ]; then
      Log "Branch selection cancelled. Only the install and source paths have been created." 1
      exit 0
    fi

    # Set the branch
    if [ -z "$BRANCH" ]; then
      BRANCH="master"
    fi

    # Clone the selected version
    case "$VERSION" in
      0)
        Log "Cloning Zero branch: $BRANCH" 1
        git clone git://github.com/mangoszero/server.git "$SRCPATH/server" -b $BRANCH --recursive
        git clone git://github.com/mangoszero/database.git "$SRCPATH/database" -b $BRANCH --recursive
        ;;

      1)
        Log "Cloning One branch: $BRANCH" 1
        git clone git://github.com/mangosone/server.git "$SRCPATH/server" -b $BRANCH --recursive
        git clone git://github.com/mangosone/database.git "$SRCPATH/database" -b $BRANCH --recursive
        ;;

      2)
        Log "Cloning Two branch: $BRANCH" 1
        git clone git://github.com/mangostwo/server.git "$SRCPATH/server" -b $BRANCH --recursive
        git clone git://github.com/mangostwo/database.git "$SRCPATH/database" -b $BRANCH --recursive
        ;;

      3)
        Log "Cloning Three branch: $BRANCH" 1
        git clone git://github.com/mangosthree/server.git "$SRCPATH/server" -b $BRANCH --recursive
        git clone git://github.com/mangosthree/database.git "$SRCPATH/database" -b $BRANCH --recursive
        ;;

      4)
        Log "Cloning Four branch: $BRANCH" 1
        git clone git://github.com/mangosfour/server.git "$SRCPATH/server" -b $BRANCH --recursive
        git clone git://github.com/mangosfour/database.git "$SRCPATH/database" -b $BRANCH --recursive
        ;;

      *)
        Log "Error: Unknown release selected for cloning!" 1
        exit 1
        ;;
    esac

    # Log success
    Log "Cloned the selected repository!" 1
  fi

  # Update the local repositories if selected
  if [[ $CLONE = *1* ]]; then
    Log "Updating your local repository..." 1

    # Update the core sources
    cd "$SRCPATH/server"
    git pull

    # Now update the database sources
    cd "$SRCPATH/database"
    git pull

    # Log success
    Log "Updated the local respository!" 1
  fi

  # use existing repository
  if [[ $CLONE = *2* ]]; then
    Log "Using existing local repository" 1
  fi
}



# Function to set the build options
function GetBuildOptions()
{
  # Select build options
  OPTIONS=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" \
    --title "Build Options" \
    --checklist "Please select your build options" 0 56 7 \
    1 "Enable Debug" Off \
    2 "Use Standard Malloc" On \
    3 "Use External ACE Libraries" On \
    4 "Use PostgreSQL Instead Of MySQL/MariaDB" Off \
    5 "Build Client Tools" Off \
    6 "Use SD2" On \
    7 "Use Eluna" On \
    3>&2 2>&1 1>&3)

  if [ $? -ne 0 ]; then
    Log "Build option selection cancelled. MaNGOS sources have been cloned." 1
    return 0
  fi

  # See if debug was selected
  if [[ $OPTIONS == *1* ]]; then
    P_DEBUG="1"
  else
    P_DEBUG="0"
  fi

  # See if standard malloc was selected
  if [[ $OPTIONS == *2* ]]; then
    P_STD_MALLOC="1"
  else
    P_STD_MALLOC="0"
  fi

  # See if external ACE was selected
  if [[ $OPTIONS == *3* ]]; then
    P_ACE_EXTERNAL="1"
  else
    P_ACE_EXTERNAL="0"
  fi

  # See if PostgreSQL was selected
  if [[ $OPTIONS == *4* ]]; then
    P_PGRESQL="1"
  else
    P_PGRESQL="0"
  fi

  # See if the client tools were selected
  if [[ $OPTIONS == *5* ]]; then
    P_TOOLS="1"
  else
    P_TOOLS="0"
  fi

  # See if SD2 will be used
  if [[ $OPTIONS == *6* ]]; then
    P_SD2="1"
  else
    P_SD2="0"
  fi

  # See if Eluna will be used
  if [[ $OPTIONS == *7* ]]; then
    P_ELUNA="1"
  else
    P_ELUNA="0"
  fi

  # Verify that at least one scripting library is enabled
  if [ $P_SD2 -eq 0 ] && [ $P_ELUNA -eq 0 ]; then
    Log "Error: You must enable either SD2, Eluna, or both to build MaNGOS!" 1
    exit 1
  fi
}



# Function to build MaNGOS
function BuildMaNGOS()
{
  # Last chance to cancel building
  $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Proceed to build MaNGOS" \
    --yesno "Are you sure you want to build MaNGOS?" 8 60

  # Check the user's answer
  if [ $? -ne 0 ]; then
    Log "Cancelled by user. MaNGOS has been cloned but not built." 1
    exit 0
  fi

  # See if the build directory exists and clean up if possible
  if [ -d "$SRCPATH/server/linux" ]; then
    # See if a makefile exists and clean up
    if [ -f $SRCPATH/server/linux/Makefile ]; then
      Log "Cleaning the old build..." 1
      cd "$SRCPATH/server/linux"
      make clean
    fi
  fi

  # Attempt to create the build directory if it doesn't exist
  if [ ! -d "$SRCPATH/server/linux" ]; then
    mkdir "$SRCPATH/server/linux"

    # See if creation was successful
    if [ $? -ne 0 ]; then
      Log "Error: Failed to create the build directory!" 1
      exit 1
    fi
  fi

  # Attempt to configure and build MaNGOS
  Log "Building MaNGOS..." 0
  cd "$SRCPATH/server/linux"
  cmake .. -DDEBUG=$P_DEBUG -DUSE_STD_MALLOC=$P_STD_MALLOC -DACE_USE_EXTERNAL=$P_ACE_EXTERNAL -DPOSTGRESQL=$P_PGRESQL -DBUILD_TOOLS=$P_TOOLS -DSCRIPT_LIB_ELUNA=$P_ELUNA -DSCRIPT_LIB_SD2=$P_SD2 -DCMAKE_INSTALL_PREFIX="$INSTPATH"
  make

  # Check for an error
  if [ $? -ne 0 ]; then
    Log "There was an error building MaNGOS!" 1
    exit 1
  fi

  # Log success
  Log "MaNGOS has been built!" 0
}



# Function to install MaNGOS
function InstallMaNGOS()
{
  # Ask to install now
  $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Install MaNGOS" \
    --yesno "Do you want to install MaNGOS now?" 8 0

  # Return if no
  if [ $? -ne 0 ]; then
    $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Install MaNGOS" \
      --msgbox "You may install MaNGOS later by changing to:\n$SRCPATH/server/linux\nAnd running: make install" 24 60

    Log "MaNGOS has not been installed after being built." 1
    exit 0
  fi

  # Install MaNGOS
  cd "$SRCPATH/server/linux"
  make install

  # Make sure the install succeeded
  if [ $? -ne 0 ]; then
    Log "There was an error installing MaNGOS!" 1
    exit 1
  fi
}



# Function to apply database updates
function UpdateDatabases()
{
  local DB_HOST="$1"
  local DB_USER="$2"
  local DB_UPW="$3"
  local DB_REALM="$4"
  local DB_WORLD="$5"
  local DB_TOONS="$6"

  # Loop through the character directories
  for pDir in $SRCPATH/database/Character/Updates/Rel* ; do
    # Verify the item returned is a directory
    if [ -d "$pDir" ]; then
      # Now loop through all SQL files for the release
      for pFile in $pDir/*.sql; do
        # Verify the item is a real, existing file
        if [ ! -f "$pFile" ]; then
          continue
        fi

        # Attempt to apply the update
        mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_TOONS < "$pFile" > /dev/null 2>&1

        # Notify the user of which updates were and were not applied
        if [ $? -ne 0 ]; then
          Log "Database update \"$pFile\" was not applied!" 0
        else
          Log "Database update \"$pFile\" was successfully applied!" 0
        fi
      done
    fi
  done

  # Loop through the realm directories
  for pDir in $SRCPATH/database/Realm/Updates/Rel* ; do
    # Verify the item returned is a directory
    if [ -d "$pDir" ]; then
      # Now loop through all SQL files for the release
      for pFile in $pDir/*.sql; do
        # Verify the item is a real, existing file
        if [ ! -f "$pFile" ]; then
          continue
        fi

        # Attempt to apply the update
        mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_REALM < "$pFile" > /dev/null 2>&1

        # Notify the user of which updates were and were not applied
        if [ $? -ne 0 ]; then
          Log "Database update \"$pFile\" was not applied!" 0
        else
          Log "Database update \"$pFile\" was successfully applied!" 0
        fi
      done
    fi
  done

  # Loop through the world directories
  for pDir in $SRCPATH/database/World/Updates/Rel* ; do
    # Verify the item returned is a directory
    if [ -d "$pDir" ]; then
      # Now loop through all SQL files for the release
      for pFile in $pDir/*.sql; do
        # Verify the item is a real, existing file
        if [ ! -f "$pFile" ]; then
          continue
        fi

        # Attempt to apply the update
        mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_WORLD < "$pFile" > /dev/null 2>&1

        # Notify the user of which updates were and were not applied
        if [ $? -ne 0 ]; then
          Log "Database update \"$pFile\" was not applied!" 0
        else
          Log "Database update \"$pFile\" was successfully applied!" 0
        fi
      done
    fi
  done
}



# Function to install or reinstall the databases
function InstallDatabases()
{
  local DB_HOST="$1"
  local DB_USER="$2"
  local DB_UPW="$3"
  local DB_REALM="$4"
  local DB_WORLD="$5"
  local DB_TOONS="$6"

  # First create the realm database structure
  mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_REALM < $SRCPATH/database/Realm/Setup/realmdLoadDB.sql

  # Check for success
  if [ $? -ne 0 ]; then
    Log "There was an error creating the realm database!" 1
    return 1
  fi

  # Now create the characters database structure
  mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_TOONS < $SRCPATH/database/Character/Setup/characterLoadDB.sql

  # Check for success
  if [ $? -ne 0 ]; then
    Log "There was an error creating the characters database!" 1
    return 1
  fi

  # Next create the world database structure
  mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_TOONS < $SRCPATH/database/World/Setup/mangosLoadDB.sql

  # Check for success
  if [ $? -ne 0 ]; then
    Log "There was an error creating the world database!" 1
    return 1
  fi

  # Finally, loop through and build the world database database
  for fFile in $SRCPATH/database/World/Setup/FullDB/*.sql; do
    # Attempt to execute the SQL file
    mysql -h $DB_HOST -u $DB_USER -p$DB_UPW $DB_WORLD < $fFile

    # Check for success
    if [ $? -ne 0 ]; then
      Log "There was an error processing \"$fFile\" during database creation!" 1
      return 1
    fi
  done

  # Now apply any updates
  UpdateDatabases $DB_HOST $DB_USER $DB_UPW $DB_REALM $DB_WORLD $DB_TOONS
}



# Function to install or update the MySQL/MariaDB databases
function HandleDatabases()
{
  local DBMODE="0"
  local DB_TMP="0"
  local DB_USER="zero"
  local DB_UPW="zero"
  local DB_ADMIN="root"
  local DB_APW="root"
  local DB_HOST="localhost"
  local DBSEL="3"
  local DB_REALM="_realm"
  local DB_WORLD="_world"
  local DB_TOONS="_characters"

  # Ask the user what to do here
  DBMODE=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database Operations" \
    --menu "What would you like to do?" 0 0 3 \
    0 "Install clean databases" \
    1 "Update existing databases" \
    2 "Skip database work" \
    3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "Database operations cancelled. No modifications have been made to your databases." 1
    return 0
  fi

  # Exit if skipping
  if [ "$DBMODE" = "2" ]; then
    Log "Skipping database work. Nothing has been modified." 1
    return 0
  fi

  # Get the database user username
  DB_TMP=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database User Username" \
    --inputbox "Default: $DB_PREFIX" 8 60 3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "DB user name entry cancelled. No modifications have been made to your databases." 1
    return 0
  fi

  # Set the user username if one was specified
  if [ ! -z "$DB_TMP" ]; then
    DB_USER="$DB_TMP"
  fi

  # Get the database user password
  DB_TMP=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database User Password" \
    --passwordbox "Default: $DB_PREFIX" 8 60 3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "DB user PW entry cancelled. No modifications have been made to your databases." 1
    return 0
  fi

  # Set the user password if one was specified
  if [ ! -z "$DB_TMP" ]; then
    DB_UPW="$DB_TMP"
  fi

  # Get the database hostname or IP address
  DB_TMP=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database Hostname Or IP Address" \
    --inputbox "Default: localhost" 0 0 3>&2 2>&1 1>&3)

  # Exit if cancelled
  if [ $? -ne 0 ]; then
    Log "DB host entry cancelled. No modifications have been made to your databases." 1
    return 0
  fi

  # Set the hostname or IP address if one was specified
  if [ ! -z "$DB_TMP" ]; then
    DB_HOST="$DB_TMP"
  fi

  # Setup database names based on release
  DB_REALM="$DB_PREFIX$DB_REALM"
  DB_WORLD="$DB_PREFIX$DB_WORLD"
  DB_TOONS="$DB_PREFIX$DB_TOONS"

  # Install fresh databases if requested
  if [ "$DBMODE" = "0" ]; then
    # Ask which databases to install/reinstall
    DBSEL=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Select Databases" \
      --checklist "Select which databases should be (re)installed" 0 60 4 \
      0 "(Re)Install Realm Database" On \
      1 "(Re)Install World Database" On \
      2 "(Re)Install Characters Database" On \
      3 "(Re)Create MaNGOS DB User" On \
      3>&2 2>&1 1>&3)

    # Exit if cancelled
    if [ $? -ne 0 ]; then
      Log "DB selection cancelled. No modifications have been made to your databases." 1
      return 0
    fi

    # Get the database admin username
    DB_TMP=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database Administrator Username" \
      --inputbox "Default: root" 8 60 3>&2 2>&1 1>&3)

    # Exit if cancelled
    if [ $? -ne 0 ]; then
      Log "DB admin entry cancelled. No modifications have been made to your databases."
      return 0
    fi

    # Set the admin username if one was specified
    if [ ! -z "$DB_TMP" ]; then
      DB_ADMIN="$DB_TMP"
    fi

    # Get the database admin password
    DB_TMP=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Database Administrator Password" \
      --passwordbox "Default: root" 8 60 3>&2 2>&1 1>&3)

    # Exit if cancelled
    if [ $? -ne 0 ]; then
      Log "DB admin PW entry cancelled. No modifications have been made to your databases."
      return 0
    fi

    # Set the admin password if one was specified
    if [ ! -z "$DB_TMP" ]; then
      DB_APW="$DB_TMP"
    fi

    # Create the SQL file
    echo "-- This file was was auto-generated by getmangos.sh" > ./getmangos.sql

    # Remove and create the realm DB if selected
    if [[ $DBSEL == *0* ]]; then
      echo "DROP DATABASE IF EXISTS \`$DB_REALM\`;" >> ./getmangos.sql
      echo "CREATE DATABASE \`$DB_REALM\` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;" >> ./getmangos.sql
    fi

    # Remove and create the world DB if selected
    if [[ $DBSEL == *1* ]]; then
      echo "DROP DATABASE IF EXISTS \`$DB_WORLD\`;" >> ./getmangos.sql
      echo "CREATE DATABASE \`$DB_WORLD\` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;" >> ./getmangos.sql
    fi

    # Remove and create the character DB if selected
    if [[ $DBSEL == *2* ]]; then
      echo "DROP DATABASE IF EXISTS \`$DB_TOONS\`;" >> ./getmangos.sql
      echo "CREATE DATABASE \`$DB_TOONS\` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;" >> ./getmangos.sql
    fi

    # Remove and create the MaNGOS DB user if selected
    if [[ $DBEL == *3* ]]; then
      # Create the user with no privileges if it does not exist and drop it
      echo "GRANT USAGE ON *.* TO '$DB_USER'@'localhost';" >> ./getmangos.sql
      echo "DROP USER '$DB_USER'@'localhost';" >> ./getmangos.sql

      # Create the database user
      echo "CREATE USER '$DB_USER'@'localhost' IDENTIFIED BY '$DB_UPW';" >> ./getmangos.sql
      echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER, LOCK TABLES ON \`$DB_REALM\`.* TO '$DB_USER'@'localhost';" >> ./getmangos.sql
      echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER, LOCK TABLES ON \`$DB_WORLD\`.* TO '$DB_USER'@'localhost';" >> ./getmangos.sql
      echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER, LOCK TABLES ON \`$DB_TOONS\`.* TO '$DB_USER'@'localhost';" >> ./getmangos.sql
    fi

    # Execute the SQL file
    mysql -h $DB_HOST -u $DB_ADMIN -p$DB_APW < ./getmangos.sql

    # Validate success
    if [ $? -ne 0 ]; then
      Log "There was an error creating the databases!" 1
      return 1
    fi

    # Clean up the temporary SQL file
    rm -f ./getmangos.sql

    # Finally, populate the databases
    InstallDatabases $DB_HOST $DB_USER $DB_UPW $DB_REALM $DB_WORLD $DB_TOONS
  fi

  # Update the databases if requested
  if [ "$DBMODE" = "1" ]; then
    UpdateDatabases $DB_HOST $DB_USER $DB_UPW $DB_REALM $DB_WORLD $DB_TOONS
  fi
}



# Function to create a Code::Blocks project
function CreateCBProject
{
  # Create the dircetory if it does not exist
  if [ ! -d $SRCPATH/server/linux ]; then
    mkdir $SRCPATH/server/linux
  fi

  # Now create the C::B project
  cd $SRCPATH/server/linux
  cmake .. -G "CodeBlocks - Unix Makefiles"
}



# Prepare the log
Log "+------------------------------------------------------------------------------+" 0
Log "| MaNGOS Configuration Script                                                  |" 0
Log "| Written By: Ryan Ashley                                                      |" 0
Log "+------------------------------------------------------------------------------+" 0

# Select which dialog to use
UseDialog

# Select which activities to do
TASKS=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Select Tasks" \
  --checklist "Please select the tasks to perform" 0 56 7 \
  1 "Install Prerequisites" Off \
  2 "Set Download And Install Paths" On \
  3 "Clone Source Repositories" On \
  4 "Build MaNGOS" On \
  5 "Install MaNGOS" On \
  6 "Install Databases" Off \
  7 "Create Code::Blocks Project File" Off \
  3>&2 2>&1 1>&3)

# Verify that the options were selected
if [ $? -ne 0 ]; then
  Log "All operations cancelled. Exiting." 1
  exit 0
fi

# Install prerequisites?
if [[ $TASKS == *1* ]]; then
  GetPrerequisites
fi

# Select release and set paths?
if [[ $TASKS == *2* ]] || [[ $TASKS == *3* ]] || [[ $TASKS == *4* ]] || [[ $TASKS == *5* ]] || [[ $TASKS == *6* ]] || [[ $TASKS == *7* ]]; then
  GetRelease
  GetPaths
fi

# Clone repos?
if [[ $TASKS == *3* ]]; then
  GetMangos
fi

# Build MaNGOS?
if [[ $TASKS == *4* ]]; then
  GetBuildOptions
  BuildMaNGOS
fi

# Install MaNGOS?
if [[ $TASKS == *5* ]]; then
  InstallMaNGOS
fi

# Install databases?
if [[ $TASKS == *6* ]]; then
  HandleDatabases
fi

# Create C::B project?
if [[ $TASKS == *7* ]]; then
  CreateCBProject
fi



# Display the end message
echo
echo "================================================================================"
echo "The selected tasks have been completed. If you built or installed Mangos, please"
echo "edit your configuration files to use the database you configured for your MaNGOS"
echo "server. If you have not configured your databases yet, please do so before"
echo "starting your server for the first time."
echo "================================================================================"
exit 0
