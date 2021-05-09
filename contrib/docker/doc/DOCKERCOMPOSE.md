<h1>The "if and why" about the mangos docker-compose.yml</h1><br>
There a many ways to write an docker-compose.<br><br>

For best practice
- keep it simple as possible
- all container named
- network is defined as mangos
- every container starts automatical, but can be stopped at any time<br>
  "restart: unless-stopped"
- localtime is mapped into the container and also define in mangos.env<br>
  This is absolute recommend, because container can run in different timezones.

<h1>Alle containers depends on the database</h1>
The mariadb:latest is a debian based container.<br> Nothing needs to be rethink - debian and ubuntu - use the same command lines.

```
  mangos-db:
    container_name: mangos-db
    image: mariadb:latest
    restart: unless-stopped
    env_file:
      - mangos.env
    networks: 
      - mangos
    volumes:
      - /etc/localtime:/etc/localtime:ro
      - ./mariadb/:/var/lib/mysql
      - ./zero-database:/zero-database

```
As you can see at the last line the source from database is mapped into the container. This is normal needed once for initialization.<br>
<br>
<h1>Next moangos login server</h1>

```
  mangos-realm:
    image: mangos-realm
    build:
      context: ./app
      dockerfile: ../mangos/contrib/docker/realm/Dockerfile
    container_name: mangos-realm
    restart: unless-stopped
    ports:
      - target: 3724
        published: 3724
        protocol: tcp
        mode: host
    env_file:
      - mangos.env
    volumes:
      - /etc/localtime:/etc/localtime:ro
      - ./app/etc:/app/etc
      - ./logs:/app/logs
    depends_on:
      - mangos-db

    networks:
      - mangos
```

The part of ports is a little bit different and live on "host".<br><br>
Why? All streams are transported into the container and ip address is changed.<br>How you wanne ban clients with the right ip address?<br> The mode "host" makes it possible. In this case it can be define only onces at running the host.<br><br>
The configuration and logs are mapped as volume into the container.
<br>
<h1>Lets jump into the world server</h1>

```
  mangos:
    image: mangos
    build:
      context: app
      dockerfile: ../mangos/contrib/docker/world/Dockerfile
    container_name: mangos
    restart: unless-stopped
    ports:
      - target: 8085
        published: 8085
        protocol: tcp
        mode: host
      - 7878:7878
    env_file:
      - mangos.env
    volumes:
      - /etc/localtime:/etc/localtime:ro
      - ./app/etc:/app/etc
      - ./data:/app/data
      - ./logs:/app/logs
    depends_on:
      - mangos-db
      - mangos-realm

    networks:
      - mangos
   
```

In principal same stuff as on the realm service.<br>
In additional we need the world data as volume.

<h1>The mangos network adapter</h1>

At the end we define an network adapter for the inner communication.

```
networks:
  mangos:
    name: mangos
    driver: bridge
```

Cheers