from ubuntu:focal

RUN apt update && apt dist-upgrade -y  --no-install-recommends \
    openssl \
    libmariadb3 

RUN mkdir -p /app/bin
COPY bin/realmd /app/bin/

RUN mkdir /app/etc
COPY etc/realmd.conf.dist /app/etc/

EXPOSE 3724

WORKDIR "/app/bin"
CMD [ "./realmd"]
