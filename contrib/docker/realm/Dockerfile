from ubuntu:focal

RUN apt update && apt dist-upgrade -y  --no-install-recommends \
    libmariadb3 

RUN useradd -m -d /home/mangos -c "MaNGOS" -U mangos

RUN mkdir -p /app/bin
COPY bin/realmd /app/bin/

RUN mkdir /app/etc
COPY etc/realmd.conf.dist /app/etc/

RUN chown -R mangos:mangos /app

#USER mangos

EXPOSE 3724

WORKDIR "/app/bin"
CMD [ "./realmd"]
