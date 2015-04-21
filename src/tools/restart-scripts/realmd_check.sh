#!/bin/bash
#For Debian based distributions only
#Change the path variable that matches your own
#Do not add a trailing slash
path="/home/mangos/server/bin"
while true; do
cd $path
./mangos
wait
done
