#!/bin/bash
DAEMON=mangosd

cd /app/bin

# run daemon as mangos user
screen -dmS mangos-zero "./$DAEMON"

echo "MaNGOS zero world daemon started"

UP_AND_RUNNING=2
# mangos server still up?
while [ $UP_AND_RUNNING -gt 1 ] ;
do
     sleep 1;
     UP_AND_RUNNING="$(ps uax | grep $DAEMON  | wc -l)";
done