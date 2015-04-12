#!/bin/bash
#   _____           ____              
#  |   __|_____ _ _|    \ ___ _ _ ___ 
#  |   __|     | | |  |  | -_| | |_ -|
#  |_____|_|_|_|___|____/|___|\_/|___|
#
# Copyright (C) 2013-2015 EmuDevs <http://emudevs.com/>
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
########################
#LazyMaNGOS .01
#(from the original script "LazyMan Linux")
#
#Supported Cores:
#  MaNGOS Zero
#
#Supported Distro(s): 
# Debian Based ONLY (for now)
# 
# By: Faded
#(faded@emudevs.com)
########################

######
#Binds
######
bldred="\E[1;31;40m"
bldgreen="\E[1;32;40m"
bldyellow="\E[1;33;40m"
bldcyan="\E[1;36;40m"
bldblue="\E[1;34;40m"
bldmagena="\E[1;35;40m"
proccnt="$(grep -c ^processor /proc/cpuinfo)"
options=""

cecho ()
{
	local default_msg="No message passed."
	message=${1:-$default_msg}
	color=${2:-$white}
	echo -e "$color $message"
	tput sgr0
	return
}
header ()
{
	clear
	echo -e ""
	cecho "-------------------------" $bldgreen
	cecho "     LazyMaNGOS .01      " $bldyellow
	cecho "-------------------------" $bldgreen
	cecho "-   [www.emudevs.com]   -" $bldyellow
	cecho "-------------------------" $bldgreen
	echo -e ""
}

######
#Root Check
######
cecho "Checking if root..." $bldgreen
sleep 1
if [ "$(id -u)" != "0" ]; then
   echo -e ""
   cecho "This script can ONLY be ran as root!" 1>&2 $bldred
   exit 1
fi
sleep 1
header
choice=3
cecho "---------------------------" $bldgreen
cecho "Please choose how to proceed" $bldgreen
cecho "---------------------------" $bldgreen
cecho "1. Install MaNGOS Zero" $bldgreen
cecho "2. Remove MaNGOS Zero" $bldred
cecho "---------------------------" $bldgreen
cecho "Choose (1 or 2)" $bldyellow
echo -n "-> "
while [ $choice -eq 3 ]; do
	read choice
	if [ $choice -eq 1 ]; then
		cecho "---------------------------" $bldgreen
		cecho "1. Installing MaNGOS Zero..." $bldgreen
		cecho "---------------------------" $bldgreen
		echo -e ""
		######
		#Fetch Username & Password
		######
		echo -e ""
		cecho "---------------------------" $bldgreen
		cecho "Adding username: mangos ..." $bldgreen
		cecho "---------------------------" $bldgreen
		sleep 1
		useradd -m -d /home/mangos mangos
		echo -e ""
		cecho "---------------------------" $bldgreen
		cecho "Enter a password for mangos" $bldgreen
		cecho "Type it right or the script wont add it!" $bldyellow
		cecho "---------------------------" $bldgreen
		sleep 1
		passwd mangos
		sleep 1
		echo -e ""
		cecho "--------------------" $bldgreen
		cecho "Checking for sudo..." $bldgreen
		cecho "--------------------" $bldgreen
		sleep 1
		sudofile="/etc/sudoers"
		if [ -f "$sudofile" ]; then
			cecho "Sudoers is installed." $bldgreen
			cecho "Updating User..."  $bldgreen
			echo "mangos ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
			sleep 1
			echo ""
			cecho "User 'mangos' has been added to sudoers." $bldgreen
		else
			echo ""
			cecho "-------------------------" $bldgreen
			cecho "Sudoers is NOT installed." $bldred
			cecho "Attempting to install..." $bldgreen
			cecho "-------------------------" $bldgreen
			if [ $distro = "rh" ]; then
				yum -y install sudo 
				sleep 1
				cecho "--------------------------" $bldgreen
				cecho "Making sure sudo exists..." $bldgreen
				cecho "--------------------------" $bldgreen
				if [ -f "$sudofile" ]; then
					echo "mangos ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
					sleep 1
					cecho "Sudo exists" $bldgreen
					cecho "User 'mangos' has been added to sudoers." $bldgreen
					echo -e ""
				else
					cecho "Still unable to locate sudo." $bldred
					cecho "Please make sure /etc/sudoers exists" $bldyellow
					cecho "Or contact the coder, (Faded@nomsoftware.com)." $bldyellow
				fi
			else
				if [ $distro = "deb" ]; then
					apt-get -y install sudo 
					sleep 1
					cecho "Making sure sudo exists..." $bldyellow
					if [ -f "$sudofile" ]; then
						echo "mangos ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
						sleep 1
						cecho "Sudo exists" $bldgreen
						cecho "User 'mangos' has been added to sudoers." $bldgreen
						echo -e ""
					else
						cecho "Still unable to locate sudo." $bldred
						cecho "Please make sure /etc/sudoers exists" $bldyellow
						cecho "Or contact the coder, (faded@emudevs.com)." $bldyellow
					fi
				fi
			fi
		fi
		echo -e ""
		cecho "Preparing the server..." $bldgreen
		echo -e "This may take a moment."
		sleep 4

		######
		#MaNGOS Zero Core Prepare
		######
		cecho "-------------------------" $bldgreen
		cecho "Installing cmake tools..." $bldgreen
		cecho "-------------------------" $bldgreen
		sleep 1
		apt-get -y install cmake 
		apt-get -y install cmake-qt-gui 
		cecho "-------------------------" $bldgreen
		cecho "Installing git and compilers..." $bldgreen
		cecho "-------------------------" $bldgreen
		sleep 1
		apt-get -y install git 
		apt-get -y install g++ 
		apt-get -y install gcc 
		apt-get -y install make 
		apt-get -y install autoconf
		cecho "-------------------------" $bldgreen
		cecho "Installing required libraries..." $bldgreen
		cecho "-------------------------" $bldgreen
		sleep 1
		apt-get -y install libace-ssl-dev 
		apt-get -y install libace-dev
		apt-get -y install libbz2-dev 
		apt-get -y install libmysql++-dev 
		apt-get -y install libmysqlclient-dev 
		apt-get -y install libssl-dev
		apt-get -y install zlib1g-dev
		apt-get -y install libtool
		cecho "-------------------------" $bldgreen
		cecho "Installing mysql..." $bldgreen
		cecho "-------------------------" $bldgreen
		sleep 1
		apt-get -y install mysql-client 
		apt-get -y install mysql-common 
		apt-get -y install mysql-server
		cecho "---------------" $bldgreen
		cecho "Finishing up..." $bldgreen
		cecho "---------------" $bldgreen
		sleep 1
		apt-get -y install bash 
		apt-get -y install screen 
		apt-get -y install wget 

		######
		#CleanUp & Install
		######
		echo -e ""
		cecho "--------------------" $bldgreen
		cecho "Cloning Zero...     " $bldgreen
		cecho "--------------------" $bldgreen
		sleep 1
		su -c "cd /home/mangos/ && git clone --recursive -b develop21 https://github.com/mangoszero/server.git" -s /bin/bash mangos
		cecho "--------------" $bldgreen
		cecho "Cloning Database..." $bldgreen
		cecho "--------------" $bldgreen
		su -c "cd /home/mangos/ && git clone --recursive -b develop21 https://github.com/mangoszero/database.git" -s /bin/bash mangos
		sleep 1
		cecho "--------------" $bldgreen
		cecho "Preparing Zero..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1

		######	
		#Get Zero Options
		######
		echo -e ""
		opt_list=9
		cecho "Please choose which options to include:" $bldgreen
		cecho "------------------------" $bldgreen
		cecho "1. Eluna (only)" $bldyellow
		cecho "2. SD2 (only)" $bldyellow
		cecho "3. Player Bots (early development)" $bldyellow
		cecho "4. Eluna + Player Bots" $bldyellow
		cecho "5. SD2 + Player Bots" $bldyellow
		cecho "6. Eluna + SD2 + Player Bots" $bldyellow
		cecho "7. Eluna + SD2 (recommended)" $bldyellow
		cecho "8. None" $bldyellow
		cecho "------------------------" $bldgreen
		cecho "Choose (1-8)" $bldyellow
		echo -n "-> "
		while [ $opt_list -eq 9 ]; do
		read opt_list
		if [ $opt_list -eq 1 ]; then
			options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=0"
			cecho "Eluna (only) selected" $bldgreen
			sleep 2
		else
			if [ $opt_list -eq 2 ]; then
				options="-DSCRIPT_LIB_SD2=1"
				cecho "SD2 (only) selected" $bldgreen
				sleep 2
			else
				if [ $opt_list -eq 3 ]; then
					options="-DPLAYERBOTS=1 -DSCRIPT_LIB_SD2=0"
					cecho "Player Bots (only) selected" $bldgreen
					sleep 2
				else
					if [ $opt_list -eq 4 ]; then
						options="-DSCRIPT_LIB_ELUNA=1 -DPLAYERBOTS=1 -DSCRIPT_LIB_SD2=0"
						cecho "Eluna + Player Bots" $bldgreen
						sleep 2
					else
						if [ $opt_list -eq 5 ]; then
							options="-DSCRIPT_LIB_SD2=1 -DPLAYERBOTS=1"
							cecho "SD2 + Player Bots selected" $bldgreen
							sleep 2
						else
							if [ $opt_list -eq 6 ]; then
								options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=1 -DPLAYERBOTS=1"
								cecho "Eluna + SD2 + Player Bots selected" $bldgreen
								sleep 2
							else
								if [ $opt_list -eq 7 ]; then
									options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=1"
									cecho "Eluna + SD2 selected" $bldgreen
									sleep 2
								else
									if [ $opt_list -eq 8 ]; then
										options="-DSCRIPT_LIB_SD2=0"
										cecho "None selected" $bldgreen
										sleep 2
									else
										echo -e ""
										cecho "Error: Selection not recognized." $bldred
										echo -e ""
										sleep 1
										cecho "Please choose which options to include:" $bldgreen
										cecho "------------------------" $bldgreen
										cecho "1. Eluna (only)" $bldyellow
										cecho "2. SD2 (only)" $bldyellow
										cecho "3. Player Bots (early development)" $bldyellow
										cecho "4. Eluna + Player Bots" $bldyellow
										cecho "5. SD2 + Player Bots" $bldyellow
										cecho "6. Eluna + SD2 + Player Bots" $bldyellow
										cecho "7. Eluna + SD2 (recommended)" $bldyellow
										cecho "8. None" $bldyellow
										cecho "------------------------" $bldgreen
										cecho "Choose (1-8)" $bldyellow
										echo -n "-> "
										opt_list=9
									fi
								fi
							fi
						fi
					fi
				fi
			fi
		fi
		done
		echo -e ""
		cecho "--------------" $bldgreen
		cecho "Running CMake..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		su -c "mkdir /home/mangos/server/build && cd /home/mangos/server/build/ && cmake ../ $options -DCMAKE_INSTALL_PREFIX=/home/mangos/zero" -s /bin/bash mangos
		cecho "--------------" $bldgreen
		cecho "Compiling Zero..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		if [ $proclist -eq 1 ]; then
			su -c "cd /home/mangos/server/build && make" -s /bin/bash mangos
			su -c "cd /home/mangos/server/build && make install" -s /bin/bash mangos
		else
			su -c "cd /home/mangos/server/build && make -j ${proccnt}" -s /bin/bash mangos
			su -c "cd /home/mangos/server/build && make install" -s /bin/bash mangos
		fi
		echo -e ""
		cecho "--------------------------------------" $bldgreen
		cecho "Please enter your mysql password." $bldgreen
		cecho "--------------------------------------" $bldgreen
		sleep 1
		echo -n "-> "
		read mysqlpass
		echo -e ""
		db="create database realmd; create database mangos; create database characters;
				GRANT ALL PRIVILEGES ON realmd.* TO mangos@'127.0.0.1' IDENTIFIED BY 'mangos';
				GRANT ALL PRIVILEGES ON mangos.* TO mangos@'127.0.0.1' IDENTIFIED BY 'mangos';
				GRANT ALL PRIVILEGES ON characters.* TO mangos@'127.0.0.1' IDENTIFIED BY 'mangos';
				flush privileges;"
		mysql -u root -p$mysqlpass -e "$db"
		echo -e ""
		cecho "------------------" $bldgreen
		cecho "DB's created." $bldgreen
		cecho "SQL Login: mangos" $bldyellow
		cecho "SQL Pass: mangos" $bldyellow
		cecho "^ Save this info ^" $bldyellow
		cecho "------------------" $bldgreen
		echo -e ""
		cecho "--------------" $bldgreen
		cecho "Preparing DB Updates..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		su -c "cat /home/mangos/database/World/Setup/FullDB/*.sql >> /home/mangos/database/World/Setup/FullDB/all.sql" -s /bin/bash mangos
		cecho "--------------" $bldgreen
		cecho "Importing realmd..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		mysql -u root -p$mysqlpass realmd < /home/mangos/database/Realm/Setup/realmdLoadDB.sql
		cecho "--------------" $bldgreen
		cecho "Importing characters..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		mysql -u root -p$mysqlpass characters < /home/mangos/database/Character/Setup/characterLoadDB.sql
		cecho "--------------" $bldgreen
		cecho "Importing mangos world..." $bldgreen
		cecho "--------------" $bldgreen
		sleep 1
		mysql -u root -p$mysqlpass mangos < /home/mangos/database/World/Setup/mangosdLoadDB.sql
		mysql -u root -p$mysqlpass mangos < /home/mangos/database/World/Setup/FullDB/all.sql
		cecho "--------------" $bldgreen
		cecho "Import Complete" $bldgreen
		cecho "--------------" $bldgreen
		echo -e ""
		cecho "--------------" $bldgreen
		cecho "Adding auto restart scripts" $bldgreen
		cecho "--------------" $bldgreen
		echo -e ""
		su -c "mkdir /home/mangos/zero/scripts" -s /bin/bash mangos
		echo 'while true; do
			cd /home/mangos/zero/bin
			./realmd
			wait
			done' >> /home/mangos/zero/scripts/realmd_check.sh
		echo 'SESSION="realmd"
			DAEMON="screen -d -m -S $SESSION /home/mangos/zero/scripts/realmd_check.sh"
			screen -r $SESSION -ls -q 2>&1 >/dev/null
			echo -e ""
			echo "Realmd has been launched into the background."
			echo -e ""
			if [ $? -le 10 ]; then
			echo "Restarting $DAEMON"
			$DAEMON
			fi
			wait' >> /home/mangos/zero/realmd.sh
		echo 'while true; do
			cd /home/mangos/zero/bin
			./mangos
			wait
			done
			' >> /home/mangos/zero/scripts/mangos_check.sh
		echo 'SESSION="mangos"
			DAEMON="screen -d -m -S $SESSION /home/mangos/zero/scripts/mangos_check.sh"
			screen -r $SESSION -ls -q 2>&1 >/dev/null
			echo -e ""
			echo "Mangos World has been launched into the background."
			echo -e ""
			if [ $? -le 10 ]; then
			echo "Restarting $DAEMON"
			$DAEMON
			fi
			wait' >> /home/mangos/zero/mangos.sh
		chown -R mangos:mangos /home/mangos/zero/*.sh
		su -c "chmod +x /home/mangos/zero/*.sh" -s /bin/bash mangos
		su -c "mv /home/mangos/zero/etc/realmd.conf.dist /home/mangos/zero/etc/realmd.conf" -s /bin/bash mangos
		su -c "mv /home/mangos/zero/etc/mangos.conf.dist /home/mangos/zero/etc/mangos.conf" -s /bin/bash mangos
		su -c "mv /home/mangos/zero/etc/ahbot.conf.dist /home/mangos/zero/etc/ahbot.conf" -s /bin/bash mangos
		echo -e ""
		cecho "------------------" $bldgreen
		cecho "Auto Restart Scripts" $bldgreen
		cecho "      Created      " $bldyellow
		cecho "First run ./realmd then run ./mangos" $bldyellow
		cecho "Both scripts located in /home/mangos/zero/" $bldyellow
		cecho "------------------" $bldgreen
		echo -e ""
		echo "
				########################
				#LazyMaNGOS .01
				########################
				- Important Information -
				DB User: mangos
				DB Pass: mangos
				Server Location: /home/mangos/zero
				Config Location: /home/mangos/zero/etc
				
				- What to do next -
				From here on out, ONLY run the server as mangos the user
				Edit your configuration files as needed.
				Then enter the zero/bin directory and run 
				./realmd & 
				./mangos &
				& means it will run in the background.
				
				Option 2:
				Use the auto scripts in /zero/
				run ./realmd.sh & ./mangos.sh
				
				Don't forget to add your realm to the realms
				table in the realmd database.
				
				Support: www.emudevs.com | www.getmangos.eu
				" >> /home/mangos/Lazy-README
				echo -e ""
		cecho "------------------" $bldgreen
		cecho "LazyMaNGOS Complete" $bldgreen
		cecho "Please view the README in /home/mangos/" $bldgreen
		cecho "------------------" $bldgreen
		echo -e ""
		exit
	else
		if [ $choice -eq 2 ]; then
			cecho "------------------" $bldgreen
			cecho "Removing MaNGOS Zero..." $bldgreen
			cecho "------------------" $bldgreen
			sleep 2
			deluser mangos && rm -rf /home/mangos
			cecho "------------------" $bldgreen
			cecho "MaNGOS Zero has been removed" $bldgreen
			cecho "You will need to manually remove the databases" $bldgreen
			cecho "------------------" $bldgreen
			exit
		else
			echo -e ""
			cecho "Error: Selection not recognized." $bldred
			echo -e ""
			cecho "---------------------------" $bldgreen
			cecho "Please choose how to proceed" $bldgreen
			cecho "---------------------------" $bldgreen
			cecho "1. Install MaNGOS Zero" $bldgreen
			cecho "2. Remove MaNGOS Zero" $bldred
			cecho "---------------------------" $bldgreen
			cecho "Choose (1 or 2)" $bldyellow
			echo -n "-> "
			choice=3
		fi
	fi
done
#END

