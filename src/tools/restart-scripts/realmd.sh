#!/bin/bash
#For Debian based distributions only
#Change the path variable that matches your own
#Do not add a trailing slash
path="/home/mangos/server/bin"
SESSION="realmd"
DAEMON="screen -d -m -S $SESSION $path/realmd_check.sh"
screen -r $SESSION -ls -q 2>&1 >/dev/null
echo -e ""
echo "Realmd has been launched into the background."
echo -e ""
if [ $? -le 10 ]; then
echo "Restarting $DAEMON"
$DAEMON
fi
wait
