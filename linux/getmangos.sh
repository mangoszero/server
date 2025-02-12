#!/bin/bash

###############################################################################
# MaNGOS Build Automation Script                                              #
# Written By: Ryan Ashley                                                     #
# Updated By: Cedric Servais                                                  #
# Updated By: Pysis                                                           #
# Copyright (C) 2014-2025 MaNGOS https://www.getmangos.eu/                        #
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
{
  # Commands
  {
    DLGAPP="whiptail"
    DLGAPPFZF='fzf --layout=reverse --cycle'
    CMAKE_CMD="cmake"
  }

  # Paths
  {
    CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

    ROOTPATH=$(readlink -e "$CUR_DIR/../..")

    LOGFILE="$ROOTPATH/getmangos.log"

    SRCPATH="$ROOTPATH/server"
    BUILDPATH="$SRCPATH"'_build'
    INSTPATH="$SRCPATH"'_install'

    dbDir="$ROOTPATH/database"
    dbDirChar="$dbDir/Character";
    dbDirCharSU="$dbDirChar/Setup"
    dbDirCharUpd="$dbDirChar/Updates"
    dbDirRealm="$dbDir/Realm";
    dbDirRealmSU="$dbDirRealm/Setup"
    dbDirRealmUpd="$dbDirRealm/Updates"
    dbDirWorld="$dbDir/World";
    dbDirWorldSU="$dbDirWorld/Setup"
    dbDirWorldUpd="$dbDirWorld/Updates"
  }

  # Build Options
  {
    P_TOOLS="0"

    P_ELUNA="1"
    P_SD3="1"
    P_BOTS="0"
    P_SOAP="0"

    P_DEBUG="0"
  }

  # Settings
  {
    [[ -z "$USE_FZF"              ]] && USE_FZF='false'

    [[ -z "$DRY_RUN"              ]] && DRY_RUN='false'

    [[ -z "$TASKS"                ]] && TASKS=''

    [[ -z "$VERSION"              ]] && VERSION="0"
    [[ -z "$SERVER_USER"          ]] && SERVER_USER="mangos"

    [[ -z "$KEEP_USER"            ]] && KEEP_USER='false'

    [[ -z "$SKIP_PATHS"           ]] && SKIP_PATHS='false'
    [[ -z "$SKIP_USER"            ]] && SKIP_USER='false'
    [[ -z "$SKIP_RELEASE"         ]] && SKIP_RELEASE='false'
    [[ -z "$SKIP_UNINSTALL"       ]] && SKIP_UNINSTALL='false'

    [[ -z "$AUTO_DEFAULT_OPTIONS" ]] && AUTO_DEFAULT_OPTIONS='false'
    [[ -z "$AUTO_BUILD"           ]] && AUTO_BUILD='false'
    [[ -z "$AUTO_INSTALL"         ]] && AUTO_INSTALL='false'
    [[ -z "$AUTO_CLEAN"           ]] && AUTO_CLEAN='true'
  }
}

# Functions
{
  # General
  {
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
      echo "$TIMESTAMP $1" >> "$LOGFILE"
    }

    function UseCmake3()
    {
      # set the command to cmake3 if its there
      which cmake3 >/dev/null 2>&1
      if [ $? -eq 0 ]; then
        CMAKE_CMD="cmake3"
      fi
    }

    # Function to test for dialog
    function UseDialog()
    {
      # Search for dialog
      which dialog >/dev/null 2>&1

      # See if dialog was found
      if [ $? -eq 0 ]; then
        DLGAPP="dialog"
      fi
    }

    # Function to test for fzf
    function UseFZF()
    {
      # Search for dialog
      type fzf >/dev/null 2>&1

      # See if dialog was found
      if [ $? -eq 0 ]; then
        DLGAPP="fzf"
      fi
    }

    # Function to test if the user is root or not
    function CheckRoot()
    {
      if [ "$(id -u)" != "0" ]; then
          Log "This script can only be used as root!" 1
          exit 1
      else
        Log "User is root, check passed" 0
      fi
    }

    # Function to detect the repos
    function DetectLocalRepo()
    {
      # First see if the Windows stuff is in place
      if [ ! -d "$CUR_DIR/../win" ]; then
        Log "Windows files directory does not exist, assuming repo has not been cloned." 0
        return 0
      fi

      # See if the sources directory exists
      if [ ! -d "$CUR_DIR/../src" ]; then
        Log "Source files directory does not exist, assuming repo has not been cloned." 0
        return 0
      fi

      # Check for the CMake directory
      if [ ! -d "$CUR_DIR/../cmake" ]; then
        Log "CMake directory does not exist, assuming repo has not been cloned." 0
        return 0
      fi

      # Set the default paths based on the current location
      SRCPATH=$( dirname $CUR_DIR )
      ROOTPATH=$( dirname $SRCPATH )
      BUILDPATH="$SRCPATH"'_build'
      INSTPATH="$SRCPATH"'_install'

      # Log the detected path
      Log "Detected cloned repository in $SRCPATH" 0
    }
  }

  # Tasks
  {
    # Function to install prerequisite libraries
    function GetPrerequisites
    {
      # First, we need to check the installer.
      {
        installer=0

        which apt-get >/dev/null 2>&1
        if [ $? -eq 0 ]; then
          installer=1
          # On a fresh OS boot (EC2) libace was not found without first updating
          apt-get update -y && apt-get -y install git lsb-release curl
        fi

        which yum >/dev/null 2>&1
        if [ $? -eq 0 ]; then
          installer=1

          for packageName in 'git' 'redhat-lsb' 'curl'; do
            if ! rpm -qa "name=$packageName"; then
              yum -y install "$packageName"
            fi
          done
        fi

        which aptitude >/dev/null 2>&1
        if [ $? -eq 0 ]; then
          installer=1
          aptitude -y install git lsb-release curl
        fi
      }

      # Then, let's check that we have the necessary tools to define the OS version.
      which lsb_release >/dev/null 2>&1
      if [ $? -ne 0 ]; then
        Log "Cannot define your OS distribution and version." 1
        return 0
      fi

      if [[ "$installer" != '1' ]]; then
        Log 'No installer found.  Exiting.' 1
        return 1;
      fi

      local OS=$( lsb_release -si)
      local VER=$(lsb_release -sc)
      local OS_VER=1

      # Ask the user to continue
      if [[ $DLGAPP == 'fzf' ]]; then
        CHOICE=$(
          echo -e 'Yes\nNo' \
          | $DLGAPPFZF --header 'Install the required build and development packages?'
        )
        if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
      else
        $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "Install Required Dependencies" \
          --yesno "Would you like to install the required build and development packages?" 8 60
      fi

      # Check the user's response
      if [ $? -ne 0 ]; then
        Log "User declined to install required tools and development libraries." 1
        return 0
      fi

      # Handle OS
      case ${OS} in
        "LinuxMint")
          case ${VER} in
            "sarah")
              # Linux Mint 18 - Ubuntu Xenial based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "rosa")
              # Linux Mint 17.3 - Ubuntu Trusty based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "rafaela")
              # Linux Mint 17.2 - Ubuntu Trusty based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "rebecca")
              # Linux Mint 17.1 - Ubuntu Trusty based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "qiana")
              # Linux Mint 17 - Ubuntu Trusty based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "maya")
              # Linux Mint 13 - Ubuntu Precise based
              su -c "aptitude -y install build-essential cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev" root
              ;;
            "betsy")
              # LMDE 2 - Debian Jessie based
              su -c "aptitude -y install build-essential linux-headers-$(uname -r) autoconf automake cmake libbz2-dev libace-dev libace-6.2.8 libssl-dev libmysqlclient-dev libtool zliblg-dev" root
              ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        "Ubuntu")
          case ${VER} in
            "precise")
              # Ubuntu 12.04 LTS
              su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
              ;;
            "trusty")
              # Ubuntu 14.04 LTS
              su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
              ;;
            "xenial")
              # Ubuntu 16.04 LTS
              su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
              ;;
            "yakkety")
              # Ubuntu 16.10
              su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
              ;;
        "zesty")
          # Ubuntu 17.04
          su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
          ;;
        "artful")
          # Ubuntu 17.10
          su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
          ;;
        "bionic")
          # Ubuntu 18.04 LTS
          su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
          ;;
        "disco")
          # Ubuntu 19.04
          su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
          ;;
        "focal")
          # Ubuntu 20.04
          su -c "apt-get -y install build-essential curl autoconf automake cmake libbz2-dev libace-dev libssl-dev libmysqlclient-dev libtool" root
          ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        "Debian")
          case ${VER} in
            "jessie")
              # Debian 8.0 "current"
              su -c "aptitude -y install curl build-essential autoconf automake cmake libbz2-dev libace-dev libssl-dev default-libmysqlclient-dev libtool" root
              ;;
            "stretch")
              # Debian Next
              su -c "aptitude -y install curl build-essential autoconf automake cmake libbz2-dev libace-dev libssl-dev default-libmysqlclient-dev libtool" root
              ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        "RedHatEntrepriseServer")
          case ${VER} in
            "santiago")
              # Red Hat 6.x
              su -c "yum -y install curl build-essential linux-headers-$(uname -r) autoconf automake cmake libbz2-dev libace-dev ace-6.3.3 libssl-dev libmysqlclient-dev libtool zliblg-dev" root
              ;;
            "maipo")
              # Red Hat 7.x
              su -c "yum -y install curl build-essential linux-headers-$(uname -r) autoconf automake cmake libbz2-dev libace-dev ace-6.3.3 libssl-dev libmysqlclient-dev libtool zliblg-dev" root
              ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        "CentOS")
          case ${VER} in
            "Core")
              # Default CentOS - Adding necessary RPM third-party.
              rpm -Uv ftp://ftp.pbone.net/mirror/ftp5.gwdg.de/pub/opensuse/repositories/devel:/libraries:/ACE:/micro/CentOS_7/x86_64/ace-6.3.3-55.1.x86_64.rpm
              rpm -Uv ftp://rpmfind.net/linux/centos/7/os/x86_64/Packages/perl-Net-Telnet-3.03-19.el7.noarch.rpm
              rpm -Uv ftp://ftp.pbone.net/mirror/ftp5.gwdg.de/pub/opensuse/repositories/devel:/libraries:/ACE:/micro:/versioned/CentOS_7/x86_64/mpc-6.3.3-42.1.x86_64.rpm
              rpm -Uv ftp://rpmfind.net/linux/centos/7/os/x86_64/Packages/libtool-2.4.2-22.el7_3.x86_64.rpm
              rpm -Uv ftp://ftp.pbone.net/mirror/ftp5.gwdg.de/pub/opensuse/repositories/devel:/libraries:/ACE:/micro/CentOS_7/x86_64/ace-devel-6.3.3-55.1.x86_64.rpm
              su -c "yum -y install epel-release"
              su -c "yum -y install curl autoconf automake cmake3 ace-devel ace-6.3.3 openssl-devel mysql-devel libtool gcc-c++ bzip2-devel" root
              ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        "Fedora")
          case ${VER} in
            "TwentyFive")
              # Fedora 25 - Adding necessary RPM third-party.
              su -c "yum -y install autoconf automake libtool gcc-c++" root
              # Getting and building ACE. Not provided in RPM for Fedora...
              rm -rf ACE-6.3.3.tar.bz2
              rm -rf ACE_wrappers
              wget ftp://download.dre.vanderbilt.edu/previous_versions/ACE-6.3.3.tar.bz2
              tar xjvf ACE-6.3.3.tar.bz2
              export ACE_ROOT=/root/ACE_wrappers
              echo '#include "ace/config-linux.h"' >> $ACE_ROOT/ace/config.h
              # We want this to output $(ACE_ROOT) without expansion.
              # This is to be resolved during make compilation, not now.
              # shellcheck disable=SC2016
              echo 'include $(ACE_ROOT)/include/makeinclude/platform_linux.GNU' >> $ACE_ROOT/include/makeinclude/platform_macros.GNU
              echo 'INSTALL_PREFIX=/usr/local' >> $ACE_ROOT/include/makeinclude/platform_macros.GNU
              export LD_LIBRARY_PATH=$ACE_ROOT/lib:$LD_LIBRARY_PATH
              CD $ACE_ROOT
              make
              make install
              cd ~
              # Installing remaining dependencies..
              su -c "yum -y install cmake openssl-devel mariadb-devel" root
              ;;
            "Forty")
              # Fedora 40 - Adding necessary RPM third-party.
              aceVersion='7.1.0'
              if [[ -e "/usr/local/lib/libACE.so.$aceVersion" ]]; then
                Log "ACE Library version $aceVersion already present.  Skipping download, build, and install process." 1
              else
                aceURLBase='ftp://download.dre.vanderbilt.edu/previous_versions'
                acePackage="ACE-$aceVersion.tar.bz2"
                aceDirName='ACE_wrappers'
                for packageName in 'autoconf' 'automake' 'libtool' 'gcc-c++'; do
                  if ! rpm -qa "name=$packageName"; then
                    dnf -y install "$packageName"
                  fi
                done

                # Getting and building ACE.
                # Not provided by typical Fedora software repositories,
                # so downloading and building directly.

                if [[ ! -e "$acePackage" ]]; then
                  curl --remote-name "$aceURLBase/$acePackage"
                fi
                if [[ -e "$aceDirName" ]]; then
                  tar xjf "$acePackage"
                fi

                export ACE_ROOT="$PWD/$aceDirName"
                pushd "$ACE_ROOT"

                echo '#include "ace/config-linux.h"' >> 'ace/config.h'
                # We want this to output $(ACE_ROOT) without expansion.
                # This is to be resolved during make compilation, not now.
                # shellcheck disable=SC2016
                echo 'include $(ACE_ROOT)/include/makeinclude/platform_linux.GNU' >> 'include/makeinclude/platform_macros.GNU'
                echo 'INSTALL_PREFIX=/usr/local' >> 'include/makeinclude/platform_macros.GNU'

                export LD_LIBRARY_PATH=$ACE_ROOT/lib:$LD_LIBRARY_PATH

                make
                make install
                popd
              fi

              # Installing remaining dependencies..

              for packageName in 'cmake' 'openssl-devel' 'mariadb-devel'; do
                if ! rpm -qa "name=$packageName"; then
                  dnf -y install "$packageName"
                fi
              done
              ;;
            *)
              OS_VER=0
              ;;
          esac
          ;;
        *)
          OS_VER=0
          ;;
      esac

      # See if a supported OS was detected
      if [ ${OS_VER} -ne 0 ]; then
        # Log success
        Log "The development tools and libraries have been installed!" 1
      else
        # Note the error
        Log "Could not identify the current OS. Nothing was installed." 1
      fi
    }

    # Function to get the WoW version
    function GetRelease
    {
      if [[ $SKIP_VSERION == 'true' ]]; then
        if [[ $DLGAPP == 'fzf' ]]; then
          VERSION=$(
            {
              echo '0 Original Release (Vanilla)' ;
              echo '1 The Burning Crusade'        ;
              echo '2 Wrath of The Lich King'     ;
              echo '3 Cataclysm'                  ;
              echo '4 Mists of Pandaria'          ;
              echo '5 Warlords of Draenor'        ;
            } \
            | $DLGAPPFZF --header 'Select a version of WoW' \
            | cut -d' ' -f1
          );
        else
          VERSION=$(
            $DLGAPP \
            --backtitle "MaNGOS Linux Build Configuration" \
            --title "Choose WoW Release" \
            --menu "Select a version of WoW" 0 0 5 \
            0 "Original Release (Vanilla)" \
            1 "The Burning Crusade" \
            2 "Wrath of The Lich King" \
            3 "Cataclysm" \
            4 "Mists of Pandaria" \
            5 "Warlords of Draenor" \
            3>&2 2>&1 1>&3
          )
        fi

        # Exit if cancelled
        if [ $? -ne 0 -o -z "$VERSION" ]; then
          Log "Version selection cancelled by user. No changes have been made to your system." 1
          exit 0
        fi
      fi

      # Now set the correct source path if the repo has been cloned already
      DetectLocalRepo
    }

    # Function to setup the technical user
    function GetUser
    {
      local TMPUSER="$SERVER_USER"

      if [[ "$SKIP_USER"     != 'true' ]]; then
        # Set the user
        if [[ $DLGAPP == 'fzf' || $DLGAPP == 'read' ]]; then
          read -p "User to run Mangos (Default: $SERVER_USER): " TMPUSER;
        else
          TMPUSER=$(
            $DLGAPP \
            --backtitle "MaNGOS Linux Build Configuration" \
            --title "User to run Mangos" \
            --inputbox "Default: $SERVER_USER" 8 60 3>&2 2>&1 1>&3
          )
        fi

        # Exit if cancelled
        if [ $? -ne 0 ]; then
          Log "User selection was cancelled. No changes have been made to your system." 1
          exit 0
        fi

        # Change the user only if it was modified
        if [ ! -z "$TMPUSER" ]; then
          USER="$TMPUSER"
        fi
      fi

      # Validate user
      id $SERVER_USER > /dev/null 2>&1
      if [ $? -ne 0 ]; then
        Log "Creating user: $SERVER_USER" 1
        useradd -m -d "/home/$SERVER_USER" $SERVER_USER > /dev/null 2>&1

        if [ $? -ne 0 ]; then
          Log "Error: Failed to create the specified user!" 1
          exit 1
        fi

        usermod -L $SERVER_USER > /dev/null 2>&1
      elif [[ "$KEEP_USER" != 'true' ]]; then
        # User already exist, asking to keep the user
        if [[ $DLGAPP == 'fzf' ]]; then
          CHOICE=$(
            echo -e 'Yes\nNo' \
            | $DLGAPPFZF --header "Would you like to keep the user \"$SERVER_USER\"?"
          )
          if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
        else
          $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "User already exist" \
          --yesno "Would you like to keep the user \"$SERVER_USER\"?" 8 60
        fi

        if [ $? -ne 0 ]; then
          Log "Removing user: $SERVER_USER" 1
          userdel -r $SERVER_USER > /dev/null 2>&1

          Log "Creating user: $SERVER_USER" 1
          useradd -m -d /home/$SERVER_USER $SERVER_USER > /dev/null 2>&1

          if [ $? -ne 0 ]; then
            Log "Error: Failed to create the specified user!" 1
            exit 1
          fi

              usermod -L $SERVER_USER > /dev/null 2>&1
        fi
      fi

      Log "User: $SERVER_USER" 0
    }

    # Function to get the source and installation paths
    function GetPaths
    {
      local TMPPATH="$HOME"

      if [[ "$SKIP_PATHS" != 'true' ]]; then
        # Source path
        {
          # Set
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Source-Code Path (Default: $SRCPATH): " TMPPATH;
          else
            TMPPATH=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Source-Code Path" \
                --inputbox "Default: $SRCPATH" 8 60 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "Source path selection was cancelled. No changes have been made to your system." 1
            exit 0
          fi

          # Change the path only if it was modified
          if [ ! -z "$TMPPATH" ]; then
            SRCPATH="$TMPPATH"
          fi

          # Validate
          if [ ! -d "$SRCPATH" ]; then
            if [[ $DLGAPP == 'fzf' ]]; then
              CHOICE=$(
                echo -e 'Yes\nNo' \
                | $DLGAPPFZF --header "Would you like to create the directory \"$SRCPATH\"?"
              )
              if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
            else
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Path does not exist" \
                --yesno "Would you like to create the directory \"$SRCPATH\"?" 8 60
            fi

            if [ $? -eq 0 ]; then
              Log "Creating source path: $SRCPATH" 1
              mkdir -p "$SRCPATH" > /dev/null 2>&1

              # Check to see if the directory was created
              if [ $? -ne 0 ]; then
                Log "Error: Failed to create the specified source-code directory!" 1
                exit 1
              fi

              chown -R $SERVER_USER:$SERVER_USER "$SRCPATH"
            else
              Log "Source path creation cancelled. No modifications have been made to your system." 1
              exit 0
            fi
          else
            # Check for old sources
            if [ -d "$SRCPATH/server" ] || [ -d "$SRCPATH/database" ]; then
              # Ask to remove the old sources
              if [[ $DLGAPP == 'fzf' ]]; then
                CHOICE=$(
                  echo -e 'Yes\nNo' \
                  | $DLGAPPFZF --header "Would you like to remove the old sources? (Answer yes if you are cloning MaNGOS)"
                )
                if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
              else
                $DLGAPP \
                  --backtitle "MaNGOS Linux Build Configuration" \
                  --title "Path already exists" \
                  --yesno "Would you like to remove the old sources? (Answer yes if you are cloning MaNGOS)" 9 60
              fi

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
        }

        # Build path
        {
          # Set
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Build Path (Default: $BUILDPATH): " TMPPATH;
          else
            TMPPATH=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Build Path" \
                --inputbox "Default: $BUILDPATH" 8 60 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "Build path selection cancelled. Only the source path has been created." 1
            exit 0
          fi

          # Change the path only if it was modified
          if [ ! -z "$TMPPATH" ]; then
            BUILDPATH="$TMPPATH"
          fi

          # Validate
          if [ ! -d "$BUILDPATH" ]; then
            if [[ $DLGAPP == 'fzf' ]]; then
              CHOICE=$(
                echo -e 'Yes\nNo' \
                | $DLGAPPFZF --header "Would you like to create the directory \"$BUILDPATH\"?"
              )
              if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
            else
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Path does not exist" \
                --yesno "Would you like to create the directory \"$BUILDPATH\"?" 8 60
            fi

            if [ $? -eq 0 ]; then
              Log "Creating build path: $BUILDPATH" 1
              mkdir -p "$BUILDPATH" > /dev/null 2>&1

              # Check to see if the directory was created
              if [ $? -ne 0 ]; then
                Log "Error: Failed to create the specified build directory!" 1
                exit 1
              fi

              chown -R $SERVER_USER:$SERVER_USER "$BUILDPATH"
            else
              Log "Build path creation cancelled. Only the source path has been created."
              exit 0
            fi
          else
            # Check for an old installation
            if [ -d "$BUILDPATH/bin" ] || [ -d "$BUILDPATH/lib" ] || [ -d "$BUILDPATH/include" ]; then

              # Ask to remove the old installation
              if [[ $DLGAPP == 'fzf' ]]; then
                CHOICE=$(
                  echo -e 'Yes\nNo' \
                  | $DLGAPPFZF --header "Would you like to uninstall the current version of MaNGOS first?"
                )
                if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
              else
                $DLGAPP \
                  --backtitle "MaNGOS Linux Build Configuration" \
                  --title "Path already exists" \
                  --yesno "Would you like to uninstall the current version of MaNGOS first?" 0 0
              fi

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
        }

        # Installation path
        {
          # Set
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Installation Path (Default: $INSTPATH): " TMPPATH;
          else
            TMPPATH=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Installation Path" \
                --inputbox "Default: $INSTPATH" 8 60 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "Install path selection cancelled. Only the source path has been created." 1
            exit 0
          fi

          # Change the path only if it was modified
          if [ ! -z "$TMPPATH" ]; then
            INSTPATH="$TMPPATH"
          fi

          # Validate
          if [ ! -d "$INSTPATH" ]; then
            if [[ $DLGAPP == 'fzf' ]]; then
              CHOICE=$(
                echo -e 'Yes\nNo' \
                | $DLGAPPFZF --header "Would you like to create the directory \"$INSTPATH\"?"
              )
              if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
            else
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Path does not exist" \
                --yesno "Would you like to create the directory \"$INSTPATH\"?" 8 60
            fi

            if [ $? -eq 0 ];then
              Log "Creating install path: $INSTPATH" 1
              mkdir -p "$INSTPATH" > /dev/null 2>&1

              # Check to see if the directory was created
              if [ $? -ne 0 ]; then
                Log "Error: Failed to create the specified installation directory!" 1
                exit 1
              fi

              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
              Log "Install path creation cancelled. Only the source path has been created."
              exit 0
            fi
          else
            # Check for an old installation
            if [ -d "$INSTPATH/bin" ] || [ -d "$INSTPATH/lib" ] || [ -d "$INSTPATH/include" ]; then

              # Ask to remove the old installation
              if [[ $DLGAPP == 'fzf' ]]; then
                CHOICE=$(
                  echo -e 'Yes\nNo' \
                  | $DLGAPPFZF --header "Would you like to uninstall the current version of MaNGOS first?"
                )
                if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
              else
                $DLGAPP \
                  --backtitle "MaNGOS Linux Build Configuration" \
                  --title "Path already exists" \
                  --yesno "Would you like to uninstall the current version of MaNGOS first?" 0 0
              fi

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
        }
      fi

      # Log the settings
      Log "Source path: $SRCPATH" 0
      Log "Build path: $BUILDPATH" 0
      Log "Install path: $INSTPATH" 0
    }



    # Function to clone or update sources
    function GetMangos
    {
      local CLONE="0"
      local BRANCH=""

      if [[ $DLGAPP == 'fzf' ]]; then
        CLONE=$(
          {
            echo '0 Clone a fresh copy of MaNGOS'       ;
            echo '1 Update your existing copy of MaNGOS';
            echo '2 Use existing copy'                  ;
          } \
          | $DLGAPPFZF --header 'Select a repository management task.'
        );
      else
        CLONE=$(
          $DLGAPP \
            --backtitle "MaNGOS Linux Build Configuration" \
            --title "Clone or update MaNGOS" \
            --menu "Would you like to clone, update, or continue?" 0 0 3 \
          0 "Clone a fresh copy of MaNGOS" \
          1 "Update your existing copy of MaNGOS" \
          2 "Use existing copy" \
          3>&2 2>&1 1>&3
        )
      fi

      # Exit if cancelled
      if [ $? -ne 0 ]; then
        Log "Source cloning cancelled. Only the install and source paths have been created." 1
        exit 0
      fi

      # Clone from scratch if selected
      if [[ $CLONE = *0* ]]; then
        # Pull a different branch?
        case "$VERSION" in
          0)
            releases=$(curl -s 'https://api.github.com/repos/mangoszero/server/branches' | grep "name" | awk 'BEGIN{FS="\""}{print $4}' | tr '\n' ' ')
            ;;
          1)
            releases=$(curl -s 'https://api.github.com/repos/mangosone/server/branches' | grep "name" | awk 'BEGIN{FS="\""}{print $4}' | tr '\n' ' ')
            ;;
          2)
            releases=$(curl -s 'https://api.github.com/repos/mangostwo/server/branches' | grep "name" | awk 'BEGIN{FS="\""}{print $4}' | tr '\n' ' ')
            ;;
          3)
            releases=$(curl -s 'https://api.github.com/repos/mangosthree/server/branches' | grep "name" | awk 'BEGIN{FS="\""}{print $4}' | tr '\n' ' ')
            ;;
          4)
            releases=$(curl -s 'https://api.github.com/repos/mangosfour/server/branches' | grep "name" | awk 'BEGIN{FS="\""}{print $4}' | tr '\n' ' ')
            ;;
          *)
            Log "Error: Unknown version to select branch" 1
            ;;
        esac

        COUNTER=1
        RADIOLIST=""  # variable where we will keep the list entries for radiolist dialog
        for i in $releases; do
          if [ $COUNTER -eq 1 ]; then
            RADIOLIST="$RADIOLIST $COUNTER $i on "
            BRANCH=$i
          else
            RADIOLIST="$RADIOLIST $COUNTER $i off "
          fi
          let COUNTER=COUNTER+1
        done

        if [[ $DLGAPP == 'fzf' ]]; then
          read -p "Select Branch (Default: $BRANCH): " TMPBRANCH;
        else
          TMPBRANCH=$(
            $DLGAPP \
              --backtitle "MaNGOS Linux Build Configuration" \
              --title "Select Branch" \
              --radiolist "Default: $BRANCH" 0 0 $COUNTER \
              $RADIOLIST \
            3>&2 2>&1 1>&3
          )
        fi

        # Exit if cancelled
        if [ $? -ne 0 ]; then
          Log "Branch selection cancelled. Only the install and source paths have been created." 1
          exit 0
        fi

        BRANCH=$(echo $releases | awk '{print $'$TMPBRANCH'}')

        # Set the branch
        if [ -z "$BRANCH" ]; then
          BRANCH="$releases | awk '{print $1}'"
        fi

        # Clone the selected version
        case "$VERSION" in
          0)
            Log "Cloning Zero branch: $BRANCH" 1
            git clone http://github.com/mangoszero/server.git "$SRCPATH/server" -b $BRANCH --recursive
            git clone http://github.com/mangoszero/database.git "$SRCPATH/database" -b $BRANCH --recursive
            ;;

          1)
            Log "Cloning One branch: $BRANCH" 1
            git clone http://github.com/mangosone/server.git "$SRCPATH/server" -b $BRANCH --recursive
            git clone http://github.com/mangosone/database.git "$SRCPATH/database" -b $BRANCH --recursive
            ;;

          2)
            Log "Cloning Two branch: $BRANCH" 1
            git clone http://github.com/mangostwo/server.git "$SRCPATH/server" -b $BRANCH --recursive
            git clone http://github.com/mangostwo/database.git "$SRCPATH/database" -b $BRANCH --recursive
            ;;

          3)
            Log "Cloning Three branch: $BRANCH" 1
            git clone http://github.com/mangosthree/server.git "$SRCPATH/server" -b $BRANCH --recursive
            git clone http://github.com/mangosthree/database.git "$SRCPATH/database" -b $BRANCH --recursive
            ;;

          4)
            Log "Cloning Four branch: $BRANCH" 1
            git clone http://github.com/mangosfour/server.git "$SRCPATH/server" -b $BRANCH --recursive
            git clone http://github.com/mangosfour/database.git "$SRCPATH/database" -b $BRANCH --recursive
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
    function GetBuildOptions
    {
      if [[ $DLGAPP == 'fzf' ]]; then
        CHOICE=$(
          echo -e 'Yes\nNo' \
          | $DLGAPPFZF --header 'Use default build options?'
        )
        if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
      else
        $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "Automatic or specific settings" \
          --yesno "Would you like to use default build options?" 8 60
      fi

      # Check the user's response
      if [ $? -eq 0 ]; then
        Log "User declined to specify build options." 1
        return 0
      fi

      # Select build options
      if [[ $DLGAPP == 'fzf' ]]; then
        OPTIONS=$(
          {
            echo '1 Build Client Tools'                     ;
            echo '2 Use Eluna'                              ;
            echo '3 Use SD3'                                ;
            echo '4 Use Player Bots AI'                     ;
            echo '5 Use SOAP'                               ;
            echo '6 Enable Debug'                           ;
          } \
          | $DLGAPPFZF -m --header 'Please select your build options'
        );
      else
        OPTIONS=$(
          $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "Build Options" \
          --checklist "Please select your build options" 0 56 7 \
          1 "Enable Debug" Off \
          2 "Use Standard Malloc" On \
          3 "Use External ACE Libraries" On \
          4 "Use PostgreSQL Instead Of MySQL/MariaDB" Off \
          5 "Build Client Tools" On \
          6 "Use SD3" On \
          7 "Use Eluna" On \
          8 "Use SOAP" Off \
          9 "Use Player Bots AI" Off \
          3>&2 2>&1 1>&3
        )
      fi

      if [ $? -ne 0 ]; then
        Log "Build option selection cancelled." 1
        return 0
      fi

      # See if the client tools were selected
      if [[ $OPTIONS == *1* ]]; then
        P_TOOLS="1"
      else
        P_TOOLS="0"
      fi

      # See if Eluna will be used
      if [[ $OPTIONS == *2* ]]; then
        P_ELUNA="1"
      else
        P_ELUNA="0"
      fi

      # See if SD3 will be used
      if [[ $OPTIONS == *3* ]]; then
        P_SD3="1"
      else
        P_SD3="0"
      fi

      if [[ $OPTIONS == *4* ]]; then
        P_BOTS="1"
      else
        P_BOTS="0"
      fi

      # See if SOAP will be used
      if [[ $OPTIONS == *5* ]]; then
        P_SOAP="1"
      else
        P_SOAP="0"
      fi

      # See if debug was selected
      if [[ $OPTIONS == *6* ]]; then
        P_DEBUG="1"
      else
        P_DEBUG="0"
      fi

      # Verify that at least one scripting library is enabled
      if [ $P_SD3 -eq 0 ] && [ $P_ELUNA -eq 0 ]; then
        Log "Error: You must enable either SD3, Eluna, or both to build MaNGOS!" 1
        exit 1
      fi
    }

    # Function to build MaNGOS
    function BuildMaNGOS
    {
      if [[ "$AUTO_BUILD" != 'true' ]]; then
        # Last chance to cancel building
        if [[ $DLGAPP == 'fzf' ]]; then
          CHOICE=$(
            echo -e 'Yes\nNo' \
            | $DLGAPPFZF --header 'Are you sure you want to build MaNGOS?'
          )
          if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
        else
          $DLGAPP \
            --backtitle "MaNGOS Linux Build Configuration" \
            --title "Proceed to build MaNGOS" \
            --yesno "Are you sure you want to build MaNGOS?" 8 60
        fi

        # Check the user's answer
        if [ $? -ne 0 ]; then
          Log "Cancelled by user." 1
          exit 0
        fi
      fi

      if [[ "$AUTO_CLEAN" != 'false' ]]; then
        # See if the build directory exists and clean up if possible
        if [ -d "$BUILDPATH" ]; then
          # See if a makefile exists and clean up
          if [ -f $BUILDPATH/Makefile ]; then
            Log "Cleaning the old build..." 1
            cd "$BUILDPATH"
            make clean
          fi
        fi
      fi

      # Attempt to create the build directory if it doesn't exist
      if [ ! -d "$BUILDPATH" ]; then
        mkdir "$BUILDPATH"

        # See if creation was successful
        if [ $? -ne 0 ]; then
          Log "Error: Failed to create the build directory!" 1
          exit 1
        fi

        chown -R $SERVER_USER:$SERVER_USER "$BUILDPATH"
      fi

      # Attempt to configure and build MaNGOS
      Log "Building MaNGOS..." 0
      cd "$BUILDPATH"
      # make sure we are using the cmake3
      UseCmake3
      $CMAKE_CMD "$SRCPATH"                 \
        -DBUILD_TOOLS=$P_TOOLS              \
                                            \
        -DSCRIPT_LIB_ELUNA=$P_ELUNA         \
        -DSCRIPT_LIB_SD3=$P_SD3             \
        -DPLAYERBOTS=$P_BOTS                \
        -DSOAP=$P_SOAP                      \
                                            \
        -DDEBUG=$P_DEBUG                    \
                                            \
        -DCMAKE_INSTALL_PREFIX="$INSTPATH"  \
        -DCONF_INSTALL_DIR="$INSTPATH/etc"  \
      ;
      make
      chown -R $SERVER_USER:$SERVER_USER "$BUILDPATH"

      # Check for an error
      if [ $? -ne 0 ]; then
        Log "There was an error building MaNGOS!" 1
        exit 1
      fi

      # Log success
      Log "MaNGOS has been built!" 0
    }

    # Function to install MaNGOS
    function InstallMaNGOS
    {
      if [[ "$AUTO_INSTALL" != 'true' ]]; then
        # Ask to install now
        if [[ $DLGAPP == 'fzf' ]]; then
          CHOICE=$(
            echo -e 'Yes\nNo' \
            | $DLGAPPFZF --header 'Do you want to install MaNGOS now?'
          )
          if [[ "$CHOICE" == 'Yes' ]]; then (exit 0); else (exit 1); fi
        else
          $DLGAPP \
            --backtitle "MaNGOS Linux Build Configuration" \
            --title "Install MaNGOS" \
            --yesno "Do you want to install MaNGOS now?" 8 0
        fi

        # Return if no
        if [ $? -ne 0 ]; then
          if [[ $DLGAPP == 'fzf' ]]; then
            echo 'You may install MaNGOS later by navigating to:'
            echo "$BUILDPATH"
            echo 'And running: make install'
          else
            $DLGAPP \
              --backtitle "MaNGOS Linux Build Configuration" \
              --title "Install MaNGOS" \
              --msgbox "You may install MaNGOS later by navigating to:\n$BUILDPATH\nAnd running: make install" 24 60
          fi

          Log "MaNGOS has not been installed." 1
          exit 0
        fi
      fi

      # Install MaNGOS
      cd "$BUILDPATH"
      make install
      chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"

      # Make sure the install succeeded
      if [ $? -ne 0 ]; then
        Log "There was an error installing MaNGOS!" 1
        exit 1
      fi
    }

    # Function to apply database updates
    function UpdateDatabases
    {
      local DB_HOST="$1"
      local DB_TYPE="$2"
      local DB_COMMAND="$3"
      local DB_USER="$4"
      local DB_UPW="$5"
      local DB_REALM="$6"
      local DB_WORLD="$7"
      local DB_CHARS="$8"
      local DB_SEL="$9"

      local updRelChar="$(
        pushd "$dbDirCharUpd" >/dev/null;

        find              \
          -mindepth 1     \
          -maxdepth 1     \
          -type     d     \
          -iname 'Rel*'   \
          -printf '%f\n'  \
        | sort -n -r      \
        | head -n 1       \
        ;

        popd >/dev/null
      )"
      local updRelCharDir="$dbDirCharUpd/$updRelChar"
      local updRelRealm="$(
        pushd "$dbDirRealmUpd" >/dev/null;

        find              \
          -mindepth 1     \
          -maxdepth 1     \
          -type     d     \
          -iname 'Rel*'   \
          -printf '%f\n'  \
        | sort -n -r      \
        | head -n 1       \
        ;

        popd >/dev/null
      )"
      local updRelRealmDir="$dbDirRealmUpd/$updRelRealm"
      local updRelWorld="$(
        pushd "$dbDirWorldUpd" >/dev/null;

        find              \
          -mindepth 1     \
          -maxdepth 1     \
          -type     d     \
          -iname 'Rel*'   \
          -printf '%f\n'  \
        | sort -n -r      \
        | head -n 1       \
        ;

        popd >/dev/null
      )"
      local updRelWorldDir="$dbDirWorldUpd/$updRelWorld"

      if [[ $DB_SEL == *2* ]]; then
        Log '' 1
        Log "Updating the \`$DB_CHARS\` database." 1
        pushd "$updRelCharDir" >/dev/null
          for pFile in $(find -iname '*.sql' -printf '%f\n' | sort); do
            if [ ! -f "$pFile" ]; then
              continue
            fi
            # Attempt to apply the update
            $DB_COMMAND $DB_CHARS < "$pFile" > /dev/null 2>&1

            # Notify the user of which updates were and were not applied
            if [ $? -ne 0 ]; then
               Log "Database update \"$pFile\" was not applied!" 1
            else
               Log "Database update \"$pFile\" was successfully applied!" 1
            fi
          done
        popd >/dev/null
      fi

      if [[ $DB_SEL == *0* ]]; then
        Log '' 1
        Log "Updating the \`$DB_REALM\` database." 1
        pushd "$updRelRealmDir" >/dev/null
          for pFile in $(find -iname '*.sql' -printf '%f\n' | sort); do
            if [ ! -f "$pFile" ]; then
              continue
            fi
            # Attempt to apply the update
            $DB_COMMAND $DB_REALM < "$pFile" > /dev/null 2>&1

            # Notify the user of which updates were and were not applied
            if [ $? -ne 0 ]; then
              Log "Database update \"$pFile\" was not applied!" 1
            else
              Log "Database update \"$pFile\" was successfully applied!" 1
            fi
          done
        popd >/dev/null
      fi

      if [[ $DB_SEL == *1* ]]; then
        Log '' 1
        Log "Updating the \`$DB_WORLD\` database." 1
        pushd "$updRelWorldDir" >/dev/null
          for pFile in $(find -iname '*.sql' -printf '%f\n' | sort); do
            if [ ! -f "$pFile" ]; then
              continue
            fi
            # Attempt to apply the update
            $DB_COMMAND $DB_WORLD < "$pFile" > /dev/null 2>&1

            # Notify the user of which updates were and were not applied
            if [ $? -ne 0 ]; then
              Log "Database update \"$pFile\" was not applied!" 1
            else
              Log "Database update \"$pFile\" was successfully applied!" 1
            fi
          done
        popd >/dev/null
      fi
    }

    # Function to install or reinstall the databases
    function InstallDatabases
    {
      local DB_HOST="$1"
      local DB_TYPE="$2"
      local DB_COMMAND="$3"
      local DB_USER="$4"
      local DB_UPW="$5"
      local DB_REALM="$6"
      local DB_WORLD="$7"
      local DB_CHARS="$8"
      local DB_SEL="$9"

      # local exisDbs=$(
      #   $DB_COMMAND -B          \
      #     --skip-column-names   \
      #     -e 'SHOW DATABASES;'  \
      #   ;
      # );

      # if [[ $exisDbs != *$DB_REALM* ]]; then
      if [[ $DB_SEL == *0* ]]; then
        # First create the realm database structure
        Log '' 1
        Log "Creating \`$DB_REALM\` database tables." 1
        $DB_COMMAND $DB_REALM < "$dbDirRealmSU/realmdLoadDB.sql"

        # Check for success
        if [ $? -ne 0 ]; then
          Log "There was an error creating the \`$DB_REALM\` database tables!" 1
          return 1
        else
          Log "The \`$DB_REALM\` database tables have been created!" 1
        fi
      fi
      # if [[ $exisDbs != *$DB_CHARS* ]]; then
      if [[ $DB_SEL == *2* ]]; then
        # Now create the characters database structure
        Log '' 1
        Log "Creating \`$DB_CHARS\` database tables." 1
        $DB_COMMAND $DB_CHARS < "$dbDirCharSU/characterLoadDB.sql"

        # Check for success
        if [ $? -ne 0 ]; then
          Log "There was an error creating the \`$DB_CHARS\` database tables!" 1
          return 1
        else
          Log "The \`$DB_CHARS\` database tables have been created!" 1
        fi
      fi
      # if [[ $exisDbs != *$DB_WORLD* ]]; then
      if [[ $DB_SEL == *1* ]]; then
        # Next create the world database structure
        Log '' 1
        Log "Creating \`$DB_WORLD\` database tables." 1
        $DB_COMMAND $DB_WORLD < "$dbDirWorldSU/mangosdLoadDB.sql"

        # Check for success
        if [ $? -ne 0 ]; then
          Log "There was an error creating the \`$DB_WORLD\` database tables!" 1
          return 1
        else
          Log "The \`$DB_WORLD\` database tables have been created!" 1
        fi
      fi

      # local exisDBWDBVTbl=$(
      #   $DB_COMMAND -B          \
      #     --skip-column-names   \
      #     "$DB_WORLD"           \
      #     -e 'SHOW TABLES;'     \
      #   | grep -q 'db_version'  \
      #   && echo 'true'          \
      #   || echo 'false'         \
      #   ;
      # );
      # if [[ "$exisDBWDBVTbl" != 'true' ]]; then
      if [[ $DB_SEL == *1* ]]; then
        # Finally, loop through and build the world database database
        Log '' 1
        Log "Populating the \`$DB_WORLD\` database." 1
        pushd "$dbDirWorldSU/FullDB" >/dev/null
        for fFile in $(find -iname '*.sql' -printf '%f\n'); do
          # Attempt to execute the SQL file
          $DB_COMMAND $DB_WORLD < $fFile

          # Check for success
          if [ $? -ne 0 ]; then
            Log "There was an error processing \"$fFile\" during database creation!" 1
            return 1
          else
            Log "The file \"$fFile\" was processed properly" 1
          fi
        done
        popd >/dev/null
      fi

      # Now apply any updates
      UpdateDatabases \
        $DB_HOST      \
        $DB_TYPE      \
        "$DB_COMMAND" \
        $DB_USER      \
        $DB_UPW       \
        $DB_REALM     \
        $DB_WORLD     \
        $DB_CHARS     \
        "$DB_SEL"     \
      ;
    }

    # Function to install or update the MySQL/MariaDB databases
    function HandleDatabases
    {
      [[ -z "$DB_MODE"    ]] && local DB_MODE="0"
      [[ -z "$DB_TYPE"    ]] && local DB_TYPE="0"
      [[ -z "$DB_COMMAND" ]] && local DB_COMMAND=""
      [[ -z "$DB_USER"    ]] && local DB_USER="mangos"
      [[ -z "$DB_UPW"     ]] && local DB_UPW="mangos"
      [[ -z "$DB_HOST"    ]] && local DB_HOST="localhost"
      [[ -z "$DB_PORT"    ]] && local DB_PORT="3306"
      [[ -z "$DB_SEL"     ]] && local DB_SEL="3"
      [[ -z "$DB_REALM"   ]] && local DB_REALM="realmd"
      [[ -z "$DB_WORLD"   ]] && local DB_WORLD="world"
      [[ -z "$DB_CHARS"   ]] && local DB_CHARS="character" # Toons

      if [[ "$SKIP_DB_MODE" != 'true' ]]; then
        # Ask the user what to do here
        if [[ $DLGAPP == 'fzf' ]]; then
          DB_MODE=$(
            {
              echo '0 Install clean databases'  ;
              echo '1 Update existing databases';
              echo '2 Skip database work'       ;
            } \
            | $DLGAPPFZF --header 'Select Database Operations'
          );
        else
          DB_MODE=$(
            $DLGAPP \
              --backtitle "MaNGOS Linux Build Configuration" \
              --title "Database Operations" \
              --menu "What would you like to do?" 0 0 3 \
              0 "Install clean databases" \
              1 "Update existing databases" \
              2 "Skip database work" \
              3>&2 2>&1 1>&3
            )
        fi

        # Exit if cancelled
        if [ $? -ne 0 ]; then
          Log "Database operations cancelled. No modifications have been made to your databases." 1
          return 0
        fi
      fi

      # Exit if skipping
      if [[ "$DB_MODE" = *2* ]]; then
        Log "Skipping database work. Nothing has been modified." 1
        return 0
      fi

      if [[ -z "$DB_COMMAND" ]]; then
        if [[ "$SKIP_DB_TYPE" != 'true' ]]; then
          # Ask the user the DB type
          if [[ $DLGAPP == 'fzf' ]]; then
            DB_TYPE=$(
              {
                echo '0 MariaDB'    ;
                echo '1 MySQL'      ;
                echo '2 PostgreSQL' ;
              } \
              | $DLGAPPFZF --header 'Select Database Type'
            );
          else
            DB_TYPE=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Database Type" \
                --menu "Which database are you using?" 0 0 3 \
              0 "MariaDB"     \
              1 "MySQL"       \
              2 "PostgreSQL"  \
              3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "Database type selection cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        if [[ "$SKIP_DB_HOST" != 'true' ]]; then
          # Get the database hostname or IP address
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Database Hostname Or IP Address (Default: localhost): " DB_HOSTNEW;
          else
            DB_HOSTNEW=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Database Hostname Or IP Address" \
                --inputbox "Default: localhost" 0 0 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "DB host entry cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        # Set the hostname or IP address if one was specified
        if [ ! -z "$DB_HOSTNEW" ]; then
          DB_HOST="$DB_HOSTNEW"
        fi

        if [[ "$SKIP_DB_PORT" != 'true' ]]; then
          # Get the database port
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Database port (Default: 3306): " DB_PORTNEW;
          else
            DB_PORTNEW=$(
              $DLGAPP \
              --backtitle "MaNGOS Linux Build Configuration" \
              --title "Database port" \
              --inputbox "Default: 3306" 0 0 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "DB port entry cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        # Set the port number if one was specified
        if [ ! -z "$DB_PORTNEW" ]; then
          DB_PORT="$DB_PORTNEW"
        fi

        if [[ "$SKIP_DB_USER" != 'true' ]]; then
          # Get the database user username
          if [[ $DLGAPP == 'fzf' ]]; then
            read -p "Database User Username (Default: $DB_USER): " DB_USERNEW;
          else
            DB_USERNEW=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Database User Username" \
                --inputbox "Default: $DB_USER" 8 60 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "DB user name entry cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        # Set the user username if one was specified
        if [ ! -z "$DB_USERNEW" ]; then
          DB_USER="$DB_USERNEW"
        fi

        if [[ "$SKIP_DB_UPW" != 'true' ]]; then
          # Get the database user password
          if [[ $DLGAPP == 'fzf' ]]; then
            read -s -p "Database User Password (Default: $DB_UPW): " DB_UPWNEW;
          else
            DB_UPWNEW=$(
              $DLGAPP \
              --backtitle "MaNGOS Linux Build Configuration" \
              --title "Database User Password" \
              --passwordbox "Default: $DB_UPW" 8 60 3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "DB user PW entry cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        # Set the user password if one was specified
        if [ ! -z "$DB_UPWNEW" ]; then
          DB_UPW="$DB_UPWNEW"
        fi

        case "${DB_TYPE}" in
          "0")
              DB_COMMAND="mariadb -u${DB_USER} -p${DB_UPW}"
              ;;
          "1")
              printf "Confirm your MySQL password\t, "
              mysql_config_editor set \
                --login-path=local    \
                --host=$DB_HOST       \
                --port=$DB_PORT       \
                --user=$DB_USER       \
                --password            \
                --skip-warn
              DB_COMMAND="mysql --login-path=local -q -s"
              ;;
          "2")
              Log "Currently not supported." 1
              return 0
              DB_COMMAND="psql -u${DB_USER} -p${DB_UPW}"
              ;;
        esac
      fi

      # Setup database names based on release
      DB_REALM="$DB_REALM"
      DB_WORLD="$DB_WORLD$VERSION"
      DB_CHARS="$DB_CHARS$VERSION"

      # Install fresh databases if requested
      if [[ "$DB_MODE" == *0* ]]; then
        if [[ "$SKIP_DB_SEL" != 'true' ]]; then
          # Ask which databases to install/reinstall
          if [[ $DLGAPP == 'fzf' ]]; then
            DB_SEL=$(
              {
                echo '0 (Re)Install Realm Database'     ;
                echo '1 (Re)Install World Database'     ;
                echo '2 (Re)Install Characters Database';
                echo '3 Update the realmlist'           ;
              } \
              | $DLGAPPFZF -m --header 'Select Databases'
            );
          else
            DB_SEL=$(
              $DLGAPP \
                --backtitle "MaNGOS Linux Build Configuration" \
                --title "Select Databases" \
                --checklist "Select which databases should be (re)installed" 0 60 4 \
              0 "(Re)Install Realm Database" On \
              1 "(Re)Install World Database" On \
              2 "(Re)Install Characters Database" On \
              3 "Update the realmlist" On \
              3>&2 2>&1 1>&3
            )
          fi

          # Exit if cancelled
          if [ $? -ne 0 ]; then
            Log "DB selection cancelled. No modifications have been made to your databases." 1
            return 0
          fi
        fi

        # Remove and create the realm DB if selected
        if [[ $DB_SEL == *0* ]]; then
          $DB_COMMAND -e "DROP DATABASE IF EXISTS \`$DB_REALM\`;"
          $DB_COMMAND -e "CREATE DATABASE \`$DB_REALM\`;"

          # Check for success
          if [ $? -ne 0 ]; then
            Log '' 1
            Log "There was an error creating the \`$DB_REALM\` database!" 1
            return 1
          else
            Log '' 1
            Log "The \`$DB_REALM\` database has been created!" 1
          fi
        fi

        # Remove and create the world DB if selected
        if [[ $DB_SEL == *1* ]]; then
          $DB_COMMAND -e "DROP DATABASE IF EXISTS $DB_WORLD;"
          $DB_COMMAND -e "CREATE DATABASE $DB_WORLD;"

          # Check for success
          if [ $? -ne 0 ]; then
            Log '' 1
            Log "There was an error creating the \`$DB_WORLD\` database!" 1
            return 1
          else
            Log '' 1
            Log "The \`$DB_WORLD\` database has been created!" 1
          fi
        fi

        # Remove and create the character DB if selected
        if [[ $DB_SEL == *2* ]]; then
          $DB_COMMAND -e "DROP DATABASE IF EXISTS $DB_CHARS;"
          $DB_COMMAND -e "CREATE DATABASE $DB_CHARS;"

          # Check for success
          if [ $? -ne 0 ]; then
            Log '' 1
            Log "There was an error creating the \`$DB_CHARS\` database!" 1
            return 1
          else
            Log '' 1
            Log "The \`$DB_CHARS\` database has been created!" 1
          fi
        fi

        if [[ "$DB_SEL" == *0* || "$DB_SEL" == *1* || "$DB_SEL" == *2* ]]; then
          # Finally, populate the databases
          InstallDatabases  \
            $DB_HOST        \
            $DB_TYPE        \
            "$DB_COMMAND"   \
            $DB_USER        \
            $DB_UPW         \
            $DB_REALM       \
            $DB_WORLD       \
            $DB_CHARS       \
            "$DB_SEL"       \
          ;
        fi

        if [[ $DB_SEL == *0* ]]; then
          $DB_COMMAND -e "
            GRANT
              SELECT,
              INSERT,
              UPDATE,
              DELETE,
              CREATE,
              DROP,
              ALTER,
              LOCK TABLES,
              CREATE ROUTINE,
              ALTER ROUTINE
            ON \`$DB_REALM\`.*
            TO '$DB_USER'@'localhost';
          ";
        fi
        if [[ $DB_SEL == *1* ]]; then
          $DB_COMMAND -e "
            GRANT
              SELECT,
              INSERT,
              UPDATE,
              DELETE,
              CREATE,
              DROP,
              ALTER,
              LOCK TABLES,
              CREATE ROUTINE,
              ALTER ROUTINE
            ON \`$DB_WORLD\`.*
            TO '$DB_USER'@'localhost';
          ";
        fi
        if [[ $DB_SEL == *2* ]]; then
          $DB_COMMAND -e "
            GRANT
              SELECT,
              INSERT,
              UPDATE,
              DELETE,
              CREATE,
              DROP,
              ALTER,
              LOCK TABLES,
              CREATE ROUTINE,
              ALTER ROUTINE
            ON \`$DB_CHARS\`.*
            TO '$DB_USER'@'localhost';
          ";
        fi

        # Updating the realmlist
        if [[ $DB_SEL == *3* ]]; then
          Log '' 1
          $DB_COMMAND $DB_REALM < "$dbDir/Tools/updateRealm.sql"
        fi
      fi

      # Update the databases if requested
      if [[ "$DB_MODE" = *1* ]]; then
        UpdateDatabases \
          $DB_HOST      \
          $DB_TYPE      \
          "$DB_COMMAND" \
          $DB_USER      \
          $DB_UPW       \
          $DB_REALM     \
          $DB_WORLD     \
          $DB_CHARS     \
          "$DB_SEL"       \
        ;
      fi
    }

    # Function helper to extract resources (mmaps, vmaps, dbc, ...) from the game
    function ExtractResources
    {
      INSTGAMEPATH=$(dirname $(find /home -name "WoW.exe"| head -1 2>>/dev/null))

      if [[ $DLGAPP == 'fzf' ]]; then
        read -p "Game directory path (Default: $INSTGAMEPATH): " GAMEPATH;
      else
        GAMEPATH=$(
          $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "WoW Game Path" \
          --inputbox "Please, provide the path to your game directory. Default: $INSTGAMEPATH" 8 60 3>&2 2>&1 1>&3
        )
      fi

      if [ -z "$GAMEPATH" ]; then
        GAMEPATH="$INSTGAMEPATH"
      fi

      if [ ! -d "$GAMEPATH" ]; then
        Log "There is no game at this location" 1
        exit 1
      fi

      if [[ $DLGAPP == 'fzf' ]]; then
        ACTIONS=$(
          {
            echo '1 DBC and Maps';
            echo '2 Vmaps';
            echo '3 Mmaps';
          } \
          | $DLGAPPFZF --header 'Extractions to perform'
        );
      else
        ACTIONS=$(
          $DLGAPP \
          --backtitle "MaNGOS Linux Build Configuration" \
          --title "Select Tasks" \
          --checklist "Please select the extractions to perform" 0 70 3 \
          1 "DBC and Maps" On \
          2 "Vmaps" On \
          3 "Mmaps" On \
          3>&2 2>&1 1>&3
        )
      fi

      if [ ! -d "$INSTPATH/bin/tools" ]; then
        Log "The client tools have not been built, cannot extract data" 1
        exit 1
      fi

      #TODO What if DBC are not yet generated ??
      if [[ $ACTIONS == *1* ]]; then
        if [ -d "$GAMEPATH/dbc" ]; then
          $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "DBC and Maps were already generated" \
            --yesno "Do you want to generate them again?" 8 60

          # Check the user's answer
          if [ $? -eq 0 ]; then
            Log "Deleting DBC and Maps previously generated." 1
            rm -rf "$GAMEPATH/dbc"
            rm -rf "$GAMEPATH/maps"

            Log "Copying DBC and Maps extractor" 0
            rm -f "$GAMEPATH/map-extractor"
            cp "$INSTPATH/bin/tools/map-extractor" "$GAMEPATH"

            Log "Extracting DBC and Maps" 0
            cd "$GAMEPATH"
            ./map-extractor

            if [ $? -eq 0 ]; then
              Log "DBC and Maps are extracted" 0
              Log "Copying DBC and Maps files to installation directory" 0
              cp -R "$GAMEPATH/dbc" "$INSTPATH/bin"
              cp -R "$GAMEPATH/maps" "$INSTPATH/bin"
              rm -rf "$GAMEPATH/map-extractor"
              Log "Changing ownership of the extracted directories"
              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
              Log "There was an issue while extracting DBC and Maps!" 1
              rm -rf "$GAMEPATH/map-extractor"
              rm -rf "$GAMEPATH/dbc"
              rm -rf "$GAMEPATH/maps"
              exit 1
            fi
          else
            Log "Copying DBC and Maps files to installation directory" 0
            cp -R "$GAMEPATH/dbc" "$INSTPATH/bin"
            cp -R "$GAMEPATH/maps" "$INSTPATH/bin"
          fi
        else
        rm -rf "$GAMEPATH/map-extractor"
        cp "$INSTPATH/bin/tools/map-extractor" "$GAMEPATH"

        Log "Extracting DBC and Maps" 0
        cd "$GAMEPATH"
        ./map-extractor

        if [ $? -eq 0 ]; then
          Log "DBC and Maps are extracted" 0
          Log "Copying DBC and Maps files to installation directory" 0
          cp -R "$GAMEPATH/dbc" "$INSTPATH/bin"
              cp -R "$GAMEPATH/maps" "$INSTPATH/bin"
              rm -rf "$GAMEPATH/map-extractor"
              Log "Changing ownership of the extracted directories"
              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
              Log "There was an issue while extracting DBC and Maps!" 1
              rm -rf "$GAMEPATH/map-extractor"
              rm -rf "$GAMEPATH/dbc"
              rm -rf "$GAMEPATH/maps"
              exit 1
            fi
        fi
      fi

      if [[ $ACTIONS == *2* ]]; then
        if [ -d "$GAMEPATH/vmaps" ]; then
          $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "VMaps were already generated" \
            --yesno "Do you want to generate them again?" 8 60

          # Check the user's answer
          if [ $? -eq 0 ]; then
            Log "Deleting VMaps previously generated." 1
            rm -rf $GAMEPATH/vmaps
            Log "Copying VMaps extractor" 0
            rm -f "$GAMEPATH/vmap-extractor"
            cp "$INSTPATH/bin/tools/vmap-extractor" "$GAMEPATH"

            Log "Extracting VMaps" 0
            cd $GAMEPATH
            # Make sure there is no previous vmaps generation that cause issue.
            rm -rf Buildings
            ./vmap-extractor

            if [ $? -eq 0 ]; then
              Log "VMaps are extracted" 0
              Log "Copying VMaps files to installation directory" 0
              cp -R "$GAMEPATH/vmaps" "$INSTPATH/bin"
              rm -rf "$GAMEPATH/vmap-extractor"
              Log "Changing ownership of the extracted directories"
              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
              Log "There was an issue while extracting VMaps!" 1
              rm -rf "$GAMEPATH/vmap-extractor"
              rm -rf "$GAMEPATH/vmaps"
              exit 1
            fi
          else
            Log "Copying VMaps files to installation directory" 0
            cp -R "$GAMEPATH/vmaps" "$INSTPATH/bin"
          fi
        else
         Log "Copying VMaps extractor" 0
         rm -f "$GAMEPATH/vmap-extractor"
         cp "$INSTPATH/bin/tools/vmap-extractor" "$GAMEPATH"

         Log "Extracting VMaps" 0
         cd $GAMEPATH
         # Make sure there is no previous vmaps generation that cause issue.
         rm -rf Buildings
         ./vmap-extractor

         if [ $? -eq 0 ]; then
           Log "VMaps are extracted" 0
           Log "Copying VMaps files to installation directory" 0
           cp -R "$GAMEPATH/vmaps" "$INSTPATH/bin"
           rm -rf "$GAMEPATH/vmap-extractor"
           Log "Changing ownership of the extracted directories"
           chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
         else
           Log "There was an issue while extracting VMaps!" 1
           rm -rf "$GAMEPATH/vmap-extractor"
           rm -rf "$GAMEPATH/vmaps"
           exit 1
         fi
        fi
      fi

      if [[ $ACTIONS == *3* ]]; then
        if [ ! -d "$GAMEPATH/maps" ]; then
          Log "Error: maps files must be created to be able to generate MMaps!" 1
          exit 1
        fi

        if [ -d "$GAMEPATH/mmaps" ]; then
          $DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "MMaps were already generated" \
            --yesno "Do you want to generate them again?" 8 60

          # Check the user's answer
          if [ $? -eq 0 ]; then
            Log "Deleting MMaps previously generated." 1
            rm -rf $GAMEPATH/mmaps

            Log "Copying MMaps extractor" 0
            rm -f "$GAMEPATH/MoveMapGen.sh"
            cp "$INSTPATH/bin/tools/MoveMapGen.sh" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/offmesh.txt" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/mmap_excluded.txt" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/mmap-extractor" "$GAMEPATH"

            CPU=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Please provide the number of CPU to be used to generate MMaps (1-4)" \
             --inputbox "Default: 1" 8 80 3>&2 2>&1 1>&3)

            # User cancelled his choice, set default to 1.
            if [ $? -ne 0 ]; then
              Log "User selection was cancelled. Max CPU set to 1." 1
              CPU=1
            fi

            if [ -z "$CPU" ]; then
              Log "User didn't gave any value. Max CPU set to 1." 1
              CPU=1
            fi

            if [ "$CPU" -lt 1 ] || [ "$CPU" -gt 4 ]; then
              Log "User entered invalid value. Max CPU set to 1." 1
              CPU=1
            fi

            Log "Extracting MMaps" 0
            cd $GAMEPATH
            # Making sure we can execute the script
            chmod 700 MoveMapGen.sh
            ./MoveMapGen.sh $CPU

            if [ $? -eq 0 ]; then
              Log "MMaps are extracted" 0
              Log "Copying MMaps files to installation directory" 0
              cp -R "$GAMEPATH/mmaps" "$INSTPATH/bin"
              rm -rf "$GAMEPATH/MoveMapGen.sh"
              rm -rf "$GAMEPATH/offmesh.txt"
              rm -rf "$GAMEPATH/mmap_excluded.txt"
              rm -rf "$GAMEPATH/mmap-extractor"
              Log "Changing ownership of the extracted directories"
              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
              Log "There was an issue while extracting MMaps!" 1
              rm -rf "$GAMEPATH/MoveMapGen.sh"
              rm -rf "$GAMEPATH/mmaps"
              rm -rf "$GAMEPATH/offmesh.txt"
              rm -rf "$GAMEPATH/mmap_excluded.txt"
              rm -rf "$GAMEPATH/mmap-extractor"
              exit 1
            fi
          else
            Log "Copying MMaps files to installation directory" 0
            cp -R "$GAMEPATH/mmaps" "$INSTPATH/bin"
          fi
        else
        Log "Copying MMaps extractor" 0
            rm -f "$GAMEPATH/MoveMapGen.sh"
            cp "$INSTPATH/bin/tools/MoveMapGen.sh" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/offmesh.txt" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/mmap_excluded.txt" "$GAMEPATH"
            cp "$INSTPATH/bin/tools/mmap-extractor" "$GAMEPATH"
        CPU=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Please provide the number of CPU to be used to generate MMaps (1-4)" \
             --inputbox "Default: 1" 8 80 3>&2 2>&1 1>&3)

            # User cancelled his choice, set default to 1.
            if [ $? -ne 0 ]; then
              Log "User selection was cancelled. Max CPU set to 1." 1
              CPU=1
            fi

            if [ -z "$CPU" ]; then
              Log "User didn't gave any value. Max CPU set to 1." 1
              CPU=1
            fi

            if [ "$CPU" -lt 1 ] || [ "$CPU" -gt 4 ]; then
              Log "User entered invalid value. Max CPU set to 1." 1
              CPU=1
            fi

            Log "Extracting MMaps" 0
            cd $GAMEPATH
            # Making sure we can execute the script
            chmod 700 MoveMapGen.sh
            ./MoveMapGen.sh $CPU

            if [ $? -eq 0 ]; then
              Log "MMaps are extracted" 0
              Log "Copying MMaps files to installation directory" 0
              cp -R "$GAMEPATH/mmaps" "$INSTPATH/bin"
              rm -rf "$GAMEPATH/MoveMapGen.sh"
              rm -rf "$GAMEPATH/offmesh.txt"
              rm -rf "$GAMEPATH/mmap_excluded.txt"
              rm -rf "$GAMEPATH/mmap-extractor"
              Log "Changing ownership of the extracted directories"
              chown -R $SERVER_USER:$SERVER_USER "$INSTPATH"
            else
          Log "There was an issue while extracting MMaps!" 1
              rm -rf "$GAMEPATH/MoveMapGen.sh"
              rm -rf "$GAMEPATH/mmaps"
              rm -rf "$GAMEPATH/offmesh.txt"
              rm -rf "$GAMEPATH/mmap_excluded.txt"
              rm -rf "$GAMEPATH/mmap-extractor"
              exit 1
            fi
        fi
      fi
    }

    # Function to create a Code::Blocks project
    function CreateCBProject
    {
      # Create the dircetory if it does not exist
      if [ ! -d $BUILDPATH ]; then
        mkdir $BUILDPATH

        chown -R $SERVER_USER:$SERVER_USER "$BUILDPATH"
      fi

      # Now create the C::B project
      cd $BUILDPATH
      # make sure we are using the cmake3
      UseCmake3
      $CMAKE_CMD .. -G "CodeBlocks - Unix Makefiles"
    }
  }
}

# Prepare the log
{
  if [[ ! -e "$LOGFILE" ]]; then
    touch "$LOGFILE";
    chown -R $SERVER_USER:$SERVER_USER "$LOGFILE"
  fi

  Log "+------------------------------------------------------------------------------+" 0
  Log "| MaNGOS Configuration Script                                                  |" 0
  Log "| Written By: Ryan Ashley                                                      |" 0
  Log "| Updated By: Cedric Servais                                                   |" 0
  Log "+------------------------------------------------------------------------------+" 0
}

# Check if user who is running this is root
CheckRoot

# Select which terminal dialog application to use
if [[ $USE_FZF == 'true' ]]; then
  UseFZF
else
  UseDialog
fi

# Tasks
{
  if [[ -z $TASKS ]]; then
    # Select which activities to do
    if [[ $DLGAPP == 'fzf' ]]; then
      TASKS=$(
        {
          echo '1 Install Prerequisites'            ;
          echo '2 Set Download And Install Paths'   ;
          echo '3 Clone Source Repositories'        ;
          echo '4 Build MaNGOS'                     ;
          echo '5 Install MaNGOS'                   ;
          echo '6 Install Databases'                ;
          echo '7 Extract Resources'                ;
          echo '8 Create Code::Blocks Project File' ;
        } \
        | $DLGAPPFZF -m --header 'Select Tasks'
      );
    else
      TASKS=$($DLGAPP --backtitle "MaNGOS Linux Build Configuration" --title "Select Tasks" \
        --checklist "Please select the tasks to perform" 0 70 8 \
        1 "Install Prerequisites" On \
        2 "Set Download And Install Paths" On \
        3 "Clone Source Repositories" On \
        4 "Build MaNGOS" On \
        5 "Install MaNGOS" On \
        6 "Install Databases" On \
        7 "Extract Resources" On \
        8 "Create Code::Blocks Project File" Off \
        3>&2 2>&1 1>&3)
    fi

    # Verify that the options were selected
    if [ $? -ne 0 ]; then
      Log "All operations cancelled. Exiting." 1
      exit 0
    fi
  fi

  # Install prerequisites?
  if [[ $TASKS == *1* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo GetPrerequisites
    else
      GetPrerequisites
    fi
  fi

  # Select release and set paths?
  if [[ $TASKS == *2* ]] || [[ $TASKS == *3* ]] || [[ $TASKS == *4* ]] || [[ $TASKS == *5* ]] || [[ $TASKS == *7* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo GetUser
      echo GetRelease
    else
      GetUser
      [[ "$SKIP_RELEASE"  != 'true' ]] && GetRelease
    fi
  fi

  if [[ $TASKS == *2* ]] || [[ $TASKS == *3* ]] || [[ $TASKS == *4* ]] || [[ $TASKS == *5* ]] || [[ $TASKS == *6* ]] || [[ $TASKS == *7* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo GetPaths
    else
      GetPaths
    fi
  fi

  # Clone repos?
  if [[ $TASKS == *3* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo GetMangos
    else
      GetMangos
    fi
  fi

  # Build MaNGOS?
  if [[ $TASKS == *4* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      [[ "$AUTO_DEFAULT_OPTIONS" != 'true' ]] && echo GetBuildOptions
      echo BuildMaNGOS
    else
      [[ "$AUTO_DEFAULT_OPTIONS" != 'true' ]] && GetBuildOptions
      BuildMaNGOS
    fi
  fi

  # Install MaNGOS?
  if [[ $TASKS == *5* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo InstallMaNGOS
    else
      InstallMaNGOS
    fi
  fi

  # Install databases?
  if [[ $TASKS == *6* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo HandleDatabases
    else
      HandleDatabases
    fi
  fi

  # Extract resources from the game?
  if [[ $TASKS == *7* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo ExtractResources
    else
      ExtractResources
    fi
  fi

  # Create C::B project?
  if [[ $TASKS == *8* ]]; then
    if [[ "$DRY_RUN" == 'true' ]]; then
      echo CreateCBProject
    else
      CreateCBProject
    fi
  fi



  # Display the end message
  echo
  if [[ "$DRY_RUN" == 'true' ]]; then
    echo 'These tasks have been selected, but not completed, as the dry run setting has been specified.'
  else
    echo "================================================================================"
    echo "The selected tasks have been completed. If you built or installed Mangos, please"
    echo "edit your configuration files to use the database you configured for your MaNGOS"
    echo "server. If you have not configured your databases yet, please do so before"
    echo "starting your server for the first time."
    echo "================================================================================"
  fi
}

exit 0
