#!/bin/bash
CMAKE_COMMANDS='-DCMAKE_INSTALL_PREFIX=/app -DCONF_INSTALL_DIR=/app/etc -DBUILD_EXTRACTORS=ON -DBUILD_PLAYERBOT=ON'

# container application 
if [ ! -d "app/bin" ]; then
  mkdir -p app/bin
fi

# needed for mangos container - from docker-compose build
if [ ! -f "app/bin/mangosd-entrypoint.sh" ]; then
   cp mangos/contrib/docker/world/mangosd-entrypoint.sh app/bin/mangosd-entrypoint.sh
fi

# docker env
if [ ! -f "mangos.env" ]; then
   cp mangos/contrib/docker/mangos.env .
fi

# the compose file for all 3 containers (db /realm /world)
if [ ! -f "docker-compose.yml" ]; then
   cp mangos/contrib/docker/docker-compose.yml .
fi

# update git source
if [ -d "mangos" ]; then
   cd mangos
   git pull
   cd ..
fi

#create a build directory 
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

cmake $CMAKE_COMMANDS ../mangos
# build mangos with all available cores
make -j$(nproc --all)
# install binaries and default configurations into
# /app/bin/{tools}
# /app/etc
make install
