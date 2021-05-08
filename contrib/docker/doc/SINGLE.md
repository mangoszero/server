Example container for the mangos zero

What you need to know:

- How docker works (docker.io -  https://www.docker.com/)
- linux commandline standards
- sql statments

This example works only on  local(host) machine. For the cloud the database must be changed.
Not here documented.

1) Create the build container on ubuntu:focal
   ```
   git clone git clone https://github.com/mangoszero/server.git mangos --recursive 

   docker build mangos/contrib/docker/ -t mangos-zero-build
   ```
2) Build the source and create directory structur
   
   Create a  build script
   ```
   echo "docker run -v$(pwd)/app:/app -v $(pwd):/work " \
        "--rm -it  --entrypoint ./mangos/contrib/docker/build.sh \
        mangos-zero-build" > build.sh
        
   chmod +x build.sh
   ```
   For a rebuild next time we only need
   ```
   ./build.sh (&& docker-compose build && docker-compose reload )
   ```
   (Optional) Create an extractor container script for the maps
   ```
   echo "docker run -v$(pwd)/app:/app -v $(pwd):/work " \
        "--rm -it mangos-zero-build bash" > extract.sh

   ```
3) (Optional) Extract needed data from client<br>
   Copy the full client into you work directory like
   (here from linux lutris)
   ```
   cp -r /home/XXX/Games/WorldOfWarcraft/drive_c/WoW1.12.1 .
   ```
   Join into the extractor container (build)
   ```
   ./extract.sh
   (container) cd WoW1.12.1
   (container) cp -r ../app/bin/tools/* .
   (container) chmod +x Extractor.sh
   (container) ./Extractor.sh
   ```
   This may take a while.
   After that "exit" the container.
   ```
   mkdir data
   cd WoW1.12.1
   cp -r Buildings  dbc  maps  mmaps  vmaps ../data
   ```
4) if everything went fine we create our container by
   ```
   docker-compose build
   ```
5) Intialize the database

   We need the right database for the server.

   ```
   git clone https://github.com/mangoszero/database zero-database --recursive
   ```
   Now we can start the database container.
   ```
   docker-compose up -d mangos-db
   docker exec -it mangos-db bash
   (container) cd zero-database
   (container) ./InstallDatabases.sh

   Database (root for now)
   - host: localhost
   - user: "root"
   - password: "mangos"
   ```
6) Configure container <br>
   ```
   cd app/etc
   cp realmd.conf.dist realmd.conf
   cp mangosd.conf.dist mangosd.conf
   cp ahbot.conf.dist ahbot.conf
   ```
   For realmd change follow lines in realmd.conf<br>
   -> nano realmd.conf
   ```
   LoginDatabaseInfo      = "localhost;3306;root;mangos;realmd"
   LogsDir                = ""
   ```
   into
   ```
   LoginDatabaseInfo      = "mangos-db;3306;root;mangos;realmd"
   LogsDir                = "/app/logs"
   ```

   For mangosd we update mangosd.conf<br>
   -> nano mangosd.conf
   ```
   DataDir                      = ""
   LogsDir                      = ""
   LoginDatabaseInfo            = "127.0.0.1;3306;root;mangos;realmd"
   WorldDatabaseInfo            = "127.0.0.1;3306;root;mangos;mangos0"
   CharacterDatabaseInfo        = "127.0.0.1;3306;root;mangos;character0"
   ```
   into
   ```
   DataDir                      = "/app/data"
   LogsDir                      = "/app/logs"
   LoginDatabaseInfo            = "mangos-db;3306;root;mangos;realmd"
   WorldDatabaseInfo            = "mangos-db;3306;root;mangos;mangos0"
   CharacterDatabaseInfo        = "mangos-db;3306;root;mangos;character0"
   ```

7) Start the realmd & mangos-one container detached
   ```
   docker-compose up -d mangos-realm mangos-one
   ```
   take a look with "docker stats". Hopefully all container stay and not reloaded all the time.

8) Join the world container to create an admin user
   ```
   docker exec -it mangos-one bash
   (container) screen -r (go into screen)
   (container - screen) account create testuser mypassword
   (container - screen) strg + a + d   (detache from screen)
   (container) exit
   ```
9) Don't forget to change the realm.rtf from WoW client to localhost.