from ubuntu:focal

RUN apt update && apt dist-upgrade -y
# we need to setup tzdata otherwise focal ask for time zone
RUN DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Berlin \
    apt install -y build-essential cmake git-core libbz2-dev \
    libmariadb-dev libmariadbclient-dev libmariadb-dev-compat \
    libssl-dev 

WORKDIR /work

ENTRYPOINT /bin/bash