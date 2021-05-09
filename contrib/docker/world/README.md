<h1>The mangosd (world) Dockerfile</h1>

This Dockerfile only is used on running the mangosd service.
As you know we got a build container.<br>Most docker instance
not use this way, but this is the cleanest way.

We use ubuntu focal as base system because of longterm
```
from ubuntu:focal
```
Update the container and add neccesary parts to run the realmd binary
```
RUN apt update && apt dist-upgrade -y  --no-install-recommends \
    openssl \
    libmariadb3 
```
Create the bin directory, copy in mangosd service and tools
```
RUN mkdir -p /app/bin/tools
COPY bin/mangosd /app/bin/
COPY bin/tools/* /app/bin/tools/
```
We save the configuration the container.<br> It will not be used in
our process, but we got it handy in te container.
```
RUN mkdir /app/etc
COPY etc/mangosd.conf.dist /app/etc/
COPY etc/ahbot.conf.dist /app/etc/
```
Now we expose the ports that the client listen on the first one for the world game play. (8085). <br>This has to be declared in the database realmd
```
EXPOSE 8085
EXPOSE 7878
```
Define the app/bin as start point ...
```
WORKDIR "/app/bin"
```
We got a diffrent entrypoint - the run script.
```
ENTRYPOINT ["/usr/bin/entrypoint.sh"]
```
We can protect the server more if we run the mangosd as seperate user.
In the matter of simplicty this is a future step.
<br><br>
<h1>The mangosd-entrypoint.sh</h1>
What the script is just simple. Fire up an screen command with the daemon
and watch over it until it ends.<br>
In case there is no daemon the container will stop.<br><br>Please got in mind:<br>docker-compose "restart: unless-stopped" the container restart endless.<br><br>

```
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
```