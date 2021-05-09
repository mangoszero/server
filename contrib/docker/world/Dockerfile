from ubuntu:focal

RUN apt update && apt dist-upgrade -y  --no-install-recommends init \
    libmariadb3 mariadb-client screen \
    openssl \
    procps nano less

COPY bin/mangosd-entrypoint.sh  /usr/bin/entrypoint.sh

RUN chmod +x /usr/bin/entrypoint.sh

RUN mkdir -p /app/bin/tools
COPY bin/mangosd /app/bin/
COPY bin/tools/* /app/bin/tools/

RUN mkdir /app/etc
COPY etc/mangosd.conf.dist /app/etc/
COPY etc/ahbot.conf.dist /app/etc/

EXPOSE 8085
EXPOSE 7878

WORKDIR "/app"

ENTRYPOINT ["/usr/bin/entrypoint.sh"]
