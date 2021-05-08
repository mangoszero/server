from ubuntu:focal

RUN apt update && apt dist-upgrade -y  --no-install-recommends init \
    libmariadb3 mariadb-client screen \
    procps nano less

COPY bin/mangosd-entrypoint.sh  /usr/bin/entrypoint.sh

RUN useradd -m -d /home/mangos -c "MaNGOS" -U mangos
RUN chmod +x /usr/bin/entrypoint.sh

RUN mkdir -p /app/bin/tools
COPY bin/mangosd /app/bin/
COPY bin/tools/* /app/bin/tools/

RUN mkdir /app/etc
COPY etc/mangosd.conf.dist /app/etc/
#COPY etc/playerbot.conf.dist /app/etc/

RUN chown -R mangos:mangos /app

#USER mangos

EXPOSE 8085
EXPOSE 7878

WORKDIR "/app"

ENTRYPOINT ["/usr/bin/entrypoint.sh"]
