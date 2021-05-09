<h1>The realmd Dockerfile</h1>

This Dockerfile only is used on running the realmd service.
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
Create the bin directory and copy in realmd service
```
RUN mkdir -p /app/bin
COPY bin/realmd /app/bin/
```
We save the configuration in the container. It will not be used in
our process, but we got it handy in te container.
```
RUN mkdir /app/etc
COPY etc/realmd.conf.dist /app/etc/
```
Now we expose the port that the client login will looking for
```
EXPOSE 3724
```
Define the app/bin as start point ...
```
WORKDIR "/app/bin"
```
The run command is the realmd for sure.
```
CMD [ "./realmd"]
```
We can protect the server more if we run the realmd as seperate user.
In the matter of simplicty this is a future step.