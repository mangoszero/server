#!/bin/bash
########################
# Copyright (C) 2015 - getMaNGOS (www.getMaNGOS.eu)
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
########################
#LazyMaNGOS 0.3
########################
#
#Supported Cores:
#  MaNGOS Zero
#  MaNGOS One
#
#Supported Distro(s):
# Debian 6+ (ubuntu, linux mint)
# Redhat 5+ (CentOS, fedora)
#
# Written By: Faded (emudevs.com)
#(faded@getmangos.eu)
########################

#-------------------------------------------------------------------------------
#	WARNING: If you do not know what you are doing, do not edit below this line!
#-------------------------------------------------------------------------------
set -e
######
#Binds
######
# Colors #
RCol='\e[0m'    # Reset
#Regular		Bold			 Underline
Bla='\e[0;30m'; BBla='\e[1;30m'; UBla='\e[4;30m';
Red='\e[0;31m'; BRed='\e[1;31m'; URed='\e[4;31m';
Gre='\e[0;32m'; BGre='\e[1;32m'; UGre='\e[4;32m';
Yel='\e[0;33m'; BYel='\e[1;33m'; UYel='\e[4;33m';
Blu='\e[0;34m'; BBlu='\e[1;34m'; UBlu='\e[4;34m';
Pur='\e[0;35m'; BPur='\e[1;35m'; UPur='\e[4;35m';
Cya='\e[0;36m'; BCya='\e[1;36m'; UCya='\e[4;36m';
Whi='\e[0;37m'; BWhi='\e[1;37m'; UWhi='\e[4;37m';

proccnt="$(grep -c ^processor /proc/cpuinfo)"
options=""

###########
# Functions
###########
#Print Header
header ()
{
	clear
	echo -e "${BWhi}--------------------------------------"
	echo -e "${BGre}When you're too lazy to do it yourself"
	echo -e "${BCya}          ┻━┻︵ \(°□°)/ ︵ ┻━┻"
	echo -e "${BWhi}--------------------------------------"
	echo -e "${BGre}             LazyMaNGOS .03"
	echo -e "${BWhi}--------------------------------------"
	echo -e "${BWhi}${BYel} [www.getmangos.eu]-[www.emudevs.com]"
	echo -e "${BWhi}--------------------------------------"
	echo -e "${RCol}"
}

choose_modules()
{
	echo -e ""
	opt_list=9
	echo -e "${BWhi}------------------------"
	echo -e "${BGre}Choose sub modules to include:"
	echo -e "${BWhi}------------------------"
	echo -e "${BCya}1. Eluna (only)"
	echo -e "${BCya}2. SD2 (only)"
	echo -e "${BCya}3. Player Bots (early development)"
	echo -e "${BCya}4. Eluna + Player Bots"
	echo -e "${BCya}5. SD2 + Player Bots"
	echo -e "${BCya}6. Eluna + SD2 + Player Bots"
	echo -e "${BCya}7. Eluna + SD2 (recommended)"
	echo -e "${BCya}8. None"
	echo -e "${BWhi}------------------------"
	echo -e "Choose (1-8)"
	echo -n "-> "
	while [ $opt_list -eq 9 ]; do
		read opt_list
	if [ $opt_list -eq 1 ]; then
		options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=0"
		echo -e "${BGre}Eluna (only) selected"
		sleep 2
	else
		if [ $opt_list -eq 2 ]; then
			options="-DSCRIPT_LIB_SD2=1"
			echo -e "${BGre}SD2 (only) selected"
			sleep 2
		else
			if [ $opt_list -eq 3 ]; then
				options="-DPLAYERBOTS=1 -DSCRIPT_LIB_SD2=0"
				echo -e "${BGre}Player Bots (only) selected"
				sleep 2
			else
				if [ $opt_list -eq 4 ]; then
					options="-DSCRIPT_LIB_ELUNA=1 -DPLAYERBOTS=1 -DSCRIPT_LIB_SD2=0"
					echo -e "${BGre}Eluna + Player Bots"
					sleep 2
				else
					if [ $opt_list -eq 5 ]; then
						options="-DSCRIPT_LIB_SD2=1 -DPLAYERBOTS=1"
						echo -e "${BGre}SD2 + Player Bots selected"
						sleep 2
					else
						if [ $opt_list -eq 6 ]; then
							options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=1 -DPLAYERBOTS=1"
							echo -e "${BGre}Eluna + SD2 + Player Bots selected"
							sleep 2
						else
							if [ $opt_list -eq 7 ]; then
								options="-DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD2=1"
								echo -e "${BGre}Eluna + SD2 selected"
								sleep 2
							else
								if [ $opt_list -eq 8 ]; then
									options="-DSCRIPT_LIB_SD2=0"
									echo -e "${BGre}None selected"
									sleep 2
								else
									echo -e ""
									echo -e "Error: Selection not recognized."
									echo -e ""
									sleep 1
									echo -e "Please choose which options to include:"
									echo -e "${BWhi}------------------------"
									echo -e "${BCya}1. Eluna (only)"
									echo -e "${BCya}2. SD2 (only)"
									echo -e "${BCya}3. Player Bots (early development)"
									echo -e "${BCya}4. Eluna + Player Bots"
									echo -e "${BCya}5. SD2 + Player Bots"
									echo -e "${BCya}6. Eluna + SD2 + Player Bots"
									echo -e "${BCya}7. Eluna + SD2 (recommended)"
									echo -e "${BCya}8. None"
									echo -e "${BWhi}------------------------"
									echo -e "Choose (1-8)"
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
}

mysql_setup()
{
	echo -e ""
	echo -e "${BWhi}--------------------------------------"
	echo -e "${BGre}Please enter your mysql password."
	echo -e "${BWhi}--------------------------------------"
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
	echo -e "${BWhi}-------------------------"
	echo -e "${BGre}DB's created."
	echo -e "${BGre}SQL Login: mangos"
	echo -e "${BGre}SQL Pass: mangos"
	echo -e "^ Save this info ^"
	echo -e "${BWhi}-------------------------"
	echo -e ""
	echo -e "${BWhi}-------------------------"
	echo -e "${BGre}Preparing DB Updates..."
	echo -e "${BWhi}-------------------------"
	sleep 1
	su -c "cat /home/$user/database/World/Setup/FullDB/*.sql >> /home/$user/database/World/Setup/FullDB/all.sql" -s /bin/bash $user
	su -c "cat /home/$user/database/World/Updates/Rel21/*.sql >> /home/$user/database/World/Updates/Rel21/all.sql" -s /bin/bash $user
	su -c "cat /home/$user/database/Character/Updates/Rel21/*.sql >> /home/$user/database/Character/Updates/Rel21/all.sql" -s /bin/bash $user
	echo -e "${BWhi}-------------------------"
	echo -e "${BGre}Importing Realmd..."
	echo -e "${BWhi}-------------------------"
	sleep 1
	mysql -u root -p$mysqlpass realmd < /home/$user/database/Realm/Setup/realmdLoadDB.sql
	echo -e "${BWhi}-------------------------"
	echo -e "${BGre}Importing Characters..."
	echo -e "${BWhi}-------------------------"
	sleep 1
	mysql -u root -p$mysqlpass characters < /home/$user/database/Character/Setup/characterLoadDB.sql
	echo -e "${BWhi}-------------------------" 
	echo -e "${BGre}Importing MaNGOS World..." 
	echo -e "${BWhi}-------------------------" 
	sleep 1
	mysql -u root -p$mysqlpass mangos < /home/$user/database/World/Setup/mangosdLoadDB.sql
	mysql -u root -p$mysqlpass mangos < /home/$user/database/World/Setup/FullDB/all.sql
	echo -e "${BWhi}-------------------------" 
	echo -e "${BGre}Import Complete" 
	echo -e "${BWhi}-------------------------" 
	echo -e ""
	choice=3
	echo -e "${BWhi}-------------------------" 
	echo -e "${BGre}Would you like to apply DB updates?" 
	echo -e "${BYel}WARNING! THIS MAY ERROR!" 
	echo -e "${BGre}Yes, apply the updates" 
	echo -e "${BGre}No, finish install" 
	echo -e "${BWhi}-------------------------" 
	echo -e "Choose (1 or 2)" 
	echo -n "-> "
	while [ $choice -eq 3 ]; do
	read choice
	if [ $choice -eq 1 ]; then
		echo -e "${BWhi}-------------------------"
		echo -e "${BGre}Updating Characters..."
		echo -e "${BWhi}-------------------------"
		sleep 1
		mysql -u root -p$mysqlpass characters < /home/$user/database/Character/Updates/Rel21/all.sql
		echo -e "${BWhi}-------------------------"
		echo -e "${BGre}Updating MaNGOS World..."
		echo -e "${BWhi}-------------------------"
		sleep 1
		mysql -u root -p$mysqlpass mangos < /home/$user/database/World/Updates/Rel21/all.sql
		echo -e "${BWhi}-------------------------"
		echo -e "${BGre}Updating Realmd..."
		echo -e "${BWhi}-------------------------"
		sleep 1
		mysql -u root -p$mysqlpass realmd < /home/$user/database/Tools/updateRealm.sql
		echo -e "${BWhi}-------------------------"
		echo -e "${BGre}Updating Complete..."
		echo -e "${BWhi}-------------------------"
		sleep 1
	else
		if [ $choice -eq 2 ]; then
			echo -e "${BWhi}-------------------------"
			echo -e "${BGre}Skipping Updates..."
			echo -e "${BWhi}-------------------------"
			sleep 1
		else
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Would you like to apply DB updates?" 
			echo -e "${BYel}WARNING! THIS MAY ERROR!" 
			echo -e "${BWhi}-------------------------" 
			echo -e "Choose (y or n)" 
			echo -n "-> "
			choice=3
		fi
	fi
	done
}

mangos_install()
{
	sleep 1
	choice=5
	echo -e "${BWhi}---------------------------"
	echo -e "${BGre}Please choose which version" 
	echo -e "${BWhi}---------------------------"
	echo -e "${BCya}1. Install MaNGOS Zero" 
	echo -e "${BCya}2. Install MaNGOS One" 
	echo -e "${BCya}3. (Disabled) Install MaNGOS Two"
	echo -e "${BCya}4. (Disabled) Install MaNGOS Three" 
	echo -e "${BWhi}---------------------------" 
	echo -e "Choose (1 or 2)" 
	echo -n "-> "
	while [ $choice -eq 5 ]; do
		read choice
		if [ $choice -eq 1 ]; then
			######
			#CleanUp & Install
			######
			echo -e ""
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Cloning Zero...     " 
			echo -e "${BWhi}-------------------------"
			sleep 1
			su -c "cd /home/$user/ && git clone --recursive -b develop21 https://github.com/mangoszero/server.git" -s /bin/bash $user
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Cloning Database..." 
			echo -e "${BWhi}-------------------------" 
			su -c "cd /home/$user/ && git clone --recursive -b develop21 https://github.com/mangoszero/database.git" -s /bin/bash $user
			echo -e "${BCya}Done" 
			echo -e ""
			sleep 1
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Preparing Zero..." 
			echo -e "${BWhi}-------------------------" 
			sleep 1

			######	
			#Get Zero Options
			######
			choose_modules
			echo -e ""
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Running CMake..." 
			echo -e "${BWhi}-------------------------" 
			sleep 1
			su -c "mkdir /home/$user/server/build" -s /bin/bash $user
			su -c "cd /home/$user/server/build && cmake /home/$user/server $options -DCMAKE_INSTALL_PREFIX=/home/$user/zero" -s /bin/bash $user
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Compiling Zero..." 
			echo -e "${BWhi}-------------------------" 
			sleep 1
			su -c "cd /home/$user/server/build && make -j $proccnt" -s /bin/bash $user
			su -c "cd /home/$user/server/build && make install" -s /bin/bash $user
			##########
			#Setup SQL
			##########
			mysql_setup
			
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Adding auto restart scripts" 
			echo -e "${BWhi}-------------------------" 
			echo -e ""
			su -c "mkdir /home/${user}/zero/scripts" -s /bin/bash $user
			echo "while true; do
				cd /home/$user/zero/bin
				./realmd
				wait
				done" >> /home/$user/zero/scripts/realmd_check.sh
			echo "SESSION='realmd'
				DAEMON='screen -d -m -S $SESSION /home/$user/zero/scripts/realmd_check.sh'
				screen -r $SESSION -ls -q 2>&1 >/dev/null
				echo -e ''
				echo 'Realmd has been launched into the background.'
				echo -e ''
				if [ $? -le 10 ]; then
				echo 'Restarting $DAEMON'
				$DAEMON
				fi
				wait" >> /home/$user/zero/realmd.sh
			echo "while true; do
				cd /home/$user/zero/bin
				./mangos
				wait
				done
				" >> /home/$user/zero/scripts/mangos_check.sh
			echo "SESSION='mangos'
				DAEMON='screen -d -m -S $SESSION /home/$user/zero/scripts/mangos_check.sh'
				screen -r $SESSION -ls -q 2>&1 >/dev/null
				echo -e ''
				echo 'Mangos World has been launched into the background.'
				echo -e ''
				if [ $? -le 10 ]; then
				echo 'Restarting $DAEMON'
				$DAEMON
				fi
				wait" >> /home/$user/zero/mangos.sh
			echo "
			########################
			#LazyMaNGOS .03 Zero
			########################
			- Important Information -
			DB User: mangos
			DB Pass: mangos
			Server Location: /home/$user/zero
			Config Location: /home/$user/zero/etc
			
			- What to do next -
			From here on out, ONLY run the server as $user
			Edit your configuration files as needed.
			Then enter the zero/bin directory and run 
			./realmd & 
			./mangos &
			& means it will run in the background.
			
			Option 2:
			Use the auto scripts in /zero/
			run ./realmd.sh & ./mangos.sh
			
			Dont forget to add your realm to the realms
			table in the realmd database.
			
			Support: www.emudevs.com | www.getmangos.eu
			" >> /home/$user/Lazy-README
			chown -R $user:$user /home/$user/*
			su -c "chmod +x /home/$user/zero/*.sh" -s /bin/bash $user
			su -c "mv /home/$user/zero/etc/realmd.conf.dist /home/$user/zero/etc/realmd.conf" -s /bin/bash $user
			su -c "mv /home/$user/zero/etc/mangosd.conf.dist /home/$user/zero/etc/mangosd.conf" -s /bin/bash $user
			su -c "mv /home/$user/zero/etc/ahbot.conf.dist /home/$user/zero/etc/ahbot.conf" -s /bin/bash $user
			echo -e ""
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Auto Restart Scripts" 
			echo -e "${BGre}      Created      " 
			echo -e "${BGre}First run ./realmd then run ./mangos" 
			echo -e "${BGre}Both scripts located in /home/$user/zero/" 
			echo -e "${BWhi}-------------------------" 
			echo -e ""
			echo -e "${BWhi}---------------------------------------" 
			echo -e "${BGre}LazyMaNGOS Complete" 
			echo -e "${BGre}Please view the README in /home/$user/" 
			echo -e "${BWhi}---------------------------------------" 
			echo -e ""
			exit
		else
			if [ $choice -eq 2 ]; then
				echo -e ""
				echo -e "${BWhi}---------------------------" 
				echo -e "${BGre}Preparing for MaNGOS One" 
				echo -e "${BWhi}---------------------------" 
				echo -e ""
				sleep 2
				######
				#CleanUp & Install
				######
				echo -e ""
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Cloning MaNGOS One...     " 
				echo -e "${BWhi}-------------------------"
				sleep 1
				su -c "cd /home/$user/ && git clone --recursive -b develop21 https://github.com/mangosone/server.git" -s /bin/bash $user
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Cloning Database..." 
				echo -e "${BWhi}-------------------------" 
				su -c "cd /home/$user/ && git clone --recursive -b develop21 https://github.com/mangosone/database.git" -s /bin/bash $user
				echo -e "${BCya}Done" 
				echo -e ""
				sleep 1
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Preparing MaNGOS One..." 
				echo -e "${BWhi}-------------------------" 
				sleep 1
				######	
				#Get One Options
				######
				choose_modules
				echo -e ""
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Running CMake..." 
				echo -e "${BWhi}-------------------------" 
				sleep 1
				su -c "mkdir /home/$user/server/build" -s /bin/bash $user
				su -c "cd /home/$user/server/build && cmake ../ $options -DCMAKE_INSTALL_PREFIX=/home/$user/one" -s /bin/bash $user
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Compiling MaNGOS One..." 
				echo -e "${BWhi}-------------------------" 
				sleep 1
				su -c "cd /home/$user/server/build && make -j $proccnt" -s /bin/bash $user
				su -c "cd /home/$user/server/build && make install" -s /bin/bash $user
				##########
				#Setup SQL
				##########
				mysql_setup
	
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Adding auto restart scripts" 
				echo -e "${BWhi}-------------------------" 
				echo -e ""
				su -c "mkdir /home/$user/one/scripts" -s /bin/bash $user
				echo "while true; do
					cd /home/$user/one/bin
					./realmd
					wait
					done" >> /home/$user/one/scripts/realmd_check.sh
				echo "SESSION='realmd'
					DAEMON='screen -d -m -S $SESSION /home/$user/one/scripts/realmd_check.sh'
					screen -r $SESSION -ls -q 2>&1 >/dev/null
					echo -e ''
					echo 'Realmd has been launched into the background.'
					echo -e ""
					if [ $? -le 10 ]; then
					echo 'Restarting $DAEMON'
					$DAEMON
					fi
					wait" >> /home/$user/one/realmd.sh
				echo "while true; do
					cd /home/$user/one/bin
					./mangos
					wait
					done
					" >> /home/$user/one/scripts/mangos_check.sh
				echo "SESSION='mangos'
					DAEMON='screen -d -m -S $SESSION /home/$user/one/scripts/mangos_check.sh'
					screen -r $SESSION -ls -q 2>&1 >/dev/null
					echo -e ''
					echo 'Mangos World has been launched into the background.'
					echo -e ''
					if [ $? -le 10 ]; then
					echo 'Restarting $DAEMON'
					$DAEMON
					fi
					wait" >> /home/$user/one/mangos.sh
				echo "
				########################
				#LazyMaNGOS .03 One
				########################
				- Important Information -
				DB User: mangos
				DB Pass: mangos
				Server Location: /home/$user/one
				Config Location: /home/$user/one/etc
				
				- What to do next -
				From here on out, ONLY run the server as $user
				Edit your configuration files as needed.
				Then enter the one/bin directory and run 
				./realmd & 
				./mangos &
				& means it will run in the background.
				
				Option 2:
				Use the auto scripts in /one/
				run ./realmd.sh & ./mangos.sh
				
				Dont forget to add your realm to the realms
				table in the realmd database.
				
				Support: www.emudevs.com | www.getmangos.eu
				" >> /home/$user/Lazy-README
				chown -R $user:$user /home/$user/*
				su -c "chmod +x /home/$user/one/*.sh" -s /bin/bash $user
				su -c "mv /home/$user/one/etc/realmd.conf.dist /home/$user/one/etc/realmd.conf" -s /bin/bash $user
				su -c "mv /home/$user/one/etc/mangosd.conf.dist /home/$user/one/etc/mangosd.conf" -s /bin/bash $user
				su -c "mv /home/$user/one/etc/ahbot.conf.dist /home/$user/one/etc/ahbot.conf" -s /bin/bash $user
				echo -e ""
				echo -e "${BWhi}-------------------------" 
				echo -e "${BGre}Auto Restart Scripts" 
				echo -e "${BGre}      Created      " 
				echo -e "${BGre}First run ./realmd then run ./mangos" 
				echo -e "${BGre}Both scripts located in /home/$user/one/" 
				echo -e "${BWhi}-------------------------"
				echo -e ""
				echo -e "${BWhi}---------------------------------------" 
				echo -e "${BGre}LazyMaNGOS Complete" 
				echo -e "${BGre}Please view the README in /home/$user/" 
				echo -e "${BWhi}---------------------------------------" 
				echo -e ""
				exit
			else
				if [ $choice -eq 3 ]; then
					echo "Sorry, MaNGOS Two is currently disabled on LazyMaNGOS."
					exit 1
				else
					if [ $choice -eq 4 ]; then
						echo "Sorry, MaNGOS Three is currently disabled on LazyMaNGOS."
						exit 1
					else
						echo -e ""
						echo -e "Error: Selection not recognized." 
						echo -e ""
						echo -e "${BWhi}---------------------------" 
						echo -e "${BGre}Please choose which version" 
						echo -e "${BWhi}---------------------------" 
						echo -e "${BCya}1. Install MaNGOS Zero (Classic)" 
						echo -e "${BCya}2. Install MaNGOS One (TBC)" 
						echo -e "${BCya}3. (Disabled) Install MaNGOS Two (WotLK)" 
						echo -e "${BCya}3. (Disabled) Install MaNGOS Three (Cataclysm)"
						echo -e "${BWhi}---------------------------" 
						echo -e "Choose (1 or 2)" 
						echo -n "-> "
						choice=5
					fi
				fi
			fi
		fi
	done
}

header

echo -e "Preparing..."
sleep 2

######
#Distro Check
######
echo -e "${RCol}"
echo -e "${BWhi}-------------------------"
echo -e "${BGre}Checking OS...     "
echo -e "${BWhi}-------------------------"
sleep 1
file="/etc/debian_version"
if [ -f "$file" ]; then
	distro="debian"
	echo -e "${BGre}Debian Detected"
	echo -e "${RCol}"
else
	file="/etc/redhat-release"
	if [ -f "$file" ]; then
		distro="redhat"
		echo -e "${BGre}Redhat Detected"
		echo -e "${RCol}"
	else
		echo -e "${BRed}Unsupported OS Detected"
		echo -e "${BYel}This script only supports Debian and Redhat based distros"
		echo -e "${BYel}Exiting"
		echo -e "${RCol}"
		exit
	fi
fi

######
#Root Check
######
echo -e "${RCol}"
echo -e "${BWhi}-------------------------"
echo -e "${BGre}Checking if root...     "
echo -e "${BWhi}-------------------------"
sleep 1
if [ "$(id -u)" != "0" ]; then
	echo -e "${BRed}This script can ONLY be ran as root!" 1>&2 
	echo -e "${RCol}"
	exit
else
	echo -e "${BGre}user is root!"
	echo -e "${RCol}"
fi

#################
# Obtain Username
#################
echo -e "${RCol}"
echo -e "${BWhi}-------------------------"
echo -e "${BGre}Enter desired username (not one in use)     "
echo -e "${BWhi}-------------------------"
echo -n "-> "
read user
sleep 1


while [ $distro ]; do
########################
# Debian Based Install #
########################
	if [ $distro = "debian" ]; then
		#Prepare for all debian distros
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Preparing the server..." 
		echo -e "${BGre}This may take a moment."
		echo -e "${BWhi}-------------------------" 
		sleep 2
		######
		#Fetch Username & Password
		######
		echo -e ""
		echo -e "${BWhi}---------------------------" 
		echo -e "${BGre}Adding username: $user ..." 
		echo -e "${BWhi}---------------------------" 
		sleep 1
		useradd -m -d /home/$user $user
		usermod -L $user
		sleep 1
		echo -e ""
		echo -e "${BWhi}--------------------" 
		echo -e "${BGre}Checking for sudo..." 
		echo -e "${BWhi}--------------------" 
		sleep 1
		sudofile="/etc/sudoers"
		if [ -f "$sudofile" ]; then
			echo -e ""
			echo -e "${BGre}Sudoers is installed." 
			echo -e "${BGre}Updating User..."  
			echo "$user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
			sleep 1
			echo ""
			echo -e "${BGre}User '$user' has been added to sudoers." 
			echo -e "" 
		else
			echo ""
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Sudoers is NOT installed." 
			echo -e "${BGre}Attempting to install..." 
			echo -e "${BWhi}-------------------------" 
			apt-get -y -qq install sudo
			sleep 1
			echo -e ""
			echo -e "${BGre}Making sure sudo exists..." 
			echo -e ""
			if [ -f "$sudofile" ]; then
				echo "$user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
				sleep 1
				echo -e ""
				echo -e "${BGre}Sudo exists" 
				echo -e "${BGre}User '$user' has been added to sudoers." 
				echo -e ""
			else
				echo -e "${BRed}Still unable to locate sudo." 
				echo -e "${BGre}Please make sure /etc/sudoers exists" 
				echo -e "${BGre}Or contact the coder, (faded@getmangos.eu)." 
			fi
		fi
		######
		#MaNGOS Prepare
		######
		echo -e ""
		echo -e "${BWhi}---------------------------" 
		echo -e "${BGre}Preparing Dependencies" 
		echo -e "${BWhi}---------------------------" 
		echo -e ""
		sleep 2
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Installing cmake tools..." 
		echo -e "${BWhi}-------------------------" 
		sleep 1
		apt-get -y -qq install cmake 
		apt-get -y -qq install cmake-qt-gui
		echo -e "${BCya}Done" 
		echo -e ""
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Installing git and compilers..." 
		echo -e "${BWhi}-------------------------"
		sleep 1
		apt-get -y -qq install git
		apt-get -y -qq install g++
		apt-get -y -qq install gcc
		apt-get -y -qq install make
		apt-get -y -qq install autoconf
		echo -e "${BCya}Done" 
		echo -e ""
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Installing required libraries..." 
		echo -e "${BWhi}-------------------------"
		sleep 1
		apt-get -y -qq install libace-ssl-dev
		apt-get -y -qq install libace-dev
		apt-get -y -qq install libbz2-dev
		apt-get -y -qq install libmysql++-dev
		apt-get -y -qq install libmysqlclient-dev
		apt-get -y -qq install libssl-dev
		apt-get -y -qq install zlib1g-dev
		apt-get -y -qq install libtool
		echo -e "${BCya}Done" 
		echo -e ""
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Installing mysql..." 
		echo -e "${BWhi}-------------------------" 
		sleep 1
		apt-get -y -qq install mysql-client
		apt-get -y -qq install mysql-common
		apt-get -y -qq install mysql-server
		echo -e "${BCya}Done" 
		echo -e ""
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Finishing up..." 
		echo -e "${BWhi}-------------------------" 
		sleep 1
		apt-get -y -qq install bash
		apt-get -y -qq install screen
		apt-get -y -qq install wget
		echo -e "${BCya}Done" 
		echo -e ""
		mangos_install
	else
	########################
	# Redhat Based Install #
	########################
	if [ $distro = "redhat" ]; then
		#Prepare for all debian distros
		echo -e "${BWhi}-------------------------" 
		echo -e "${BGre}Preparing the server..." 
		echo -e "${BGre}This may take a moment."
		echo -e "${BWhi}-------------------------" 
		sleep 2
		######
		#Fetch Username & Password
		######
		echo -e ""
		echo -e "${BWhi}---------------------------" 
		echo -e "${BGre}Adding username: $user ..." 
		echo -e "${BWhi}---------------------------" 
		sleep 1
		useradd -m -d /home/$user $user
		usermod -L $user
		sleep 1
		echo -e ""
		echo -e "${BWhi}--------------------" 
		echo -e "${BGre}Checking for sudo..." 
		echo -e "${BWhi}--------------------" 
		sleep 1
		sudofile="/etc/sudoers"
		if [ -f "$sudofile" ]; then
			echo -e "" 
			echo -e "${BGre}Sudoers is installed." 
			echo -e "${BGre}Updating User..."  
			echo "mangos ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
			sleep 1
			echo ""
			echo -e "${BGre}User '$user' has been added to sudoers." 
			echo -e "" 
		else
			echo ""
			echo -e "${BWhi}-------------------------" 
			echo -e "${BGre}Sudoers is NOT installed." 
			echo -e "${BGre}Attempting to install..." 
			echo -e "${BWhi}-------------------------" 
			yum -y -q install sudo
			sleep 1
			echo -e ""
			echo -e "${BGre}Making sure sudo exists..." 
			echo -e ""
			if [ -f "$sudofile" ]; then
				echo "$user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
				sleep 1
				echo -e ""
				echo -e "${BGre}Sudo exists" 
				echo -e "${BGre}User '$user' has been added to sudoers." 
				echo -e ""
			else
				echo -e "${BRed}Still unable to locate sudo." 
				echo -e "${BGre}Please make sure /etc/sudoers exists" 
				echo -e "${BGre}Or contact the coder, (faded@getmangos.eu)." 
			fi
		fi
		######
		#MaNGOS Prepare
		######
		echo -e ""
		echo -e "${BWhi}---------------------------" 
		echo -e "${BGre}Preparing Dependencies" 
		echo -e "${BWhi}---------------------------" 
		echo -e ""
		sleep 2
		echo -e "${BWhi}---------------------------"
		echo -e "${BGre}Installing Dependencies..."
		echo -e "${BWhi}---------------------------"
		yum -y -q install git cmake openssl bzip2* openssl-devel
		yum -y -q groupinstall "Development Tools"
		echo -e "${BCya}Done"
		echo -e ""
		echo -e "${BWhi}---------------------------"
		echo -e "${BGre}Fetching & Installing MySQL..."
		echo -e "${BWhi}---------------------------"
		rpm -Uvh http://dev.mysql.com/get/mysql-community-release-el7-5.noarch.rpm
		yum -y -q install mysql-community-server
		echo -e "${BCya}Done"
		echo -e ""
		echo -e "${BWhi}---------------------------"
		echo -e "${BGre}Restarting MySQL"
		echo -e "${BWhi}---------------------------"
		systemctl enable mysqld
		systemctl start mysqld
		yum -y -q install mysql-devel
		echo -e "${BCya}Done"
		echo -e ""
		echo -e "${BWhi}---------------------------"
		echo -e "${BGre}Launching Secure MySQL Config..."
		echo -e "${BGre}Press enter for no password, set a password and say Y to all questions"
		echo -e "${BWhi}---------------------------"
		echo -e ""
		sleep 6
		mysql_secure_installation
		echo -e "${BWhi}-------------------------"
		echo -e "${BGre}Finishing up..."
		echo -e "${BWhi}-------------------------"
		sleep 1
		mangos_install
	fi
fi
done
#END
