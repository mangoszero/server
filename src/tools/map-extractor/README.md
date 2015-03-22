map extractor
--------------
The *map extractor* will extract map information from the game client.

Requirements
------------
You will need a working installation of the [World of Warcraft][1] client patched
to version 1.12.x.

Instructions - Linux
--------------------
Use the created executable to extract model information. Change the data path if
needed.

    $ map-extractor -i /mnt/windows/games/world of warcraft/

Resulting files will be in `./dbc` and `./maps`

Instructions - Windows
----------------------
Use the created executable (from command prompt) to extract model information.
It should find the data path for your client installation through the Windows
registry, but the data path can be specified with the -d option.

Resulting files will be in `.\dbc` and `.\maps`

Parameters
----------
The *map extractor* can be used with a few parameters to customize input, output
and generated output.

* `-i PATH`, `--input PATH`: set the path for reading the client's MPQ archives to the given
  path.
* `-o PATH`, `--output PATH`: set the path for writing the extracted client database files and
  the generated maps to the given path
* `-e FLAG`, `--extract FLAG`: extract/generate a specific set of data. To only extract and
  generate map files, set to `1`. To extract client database files only,
  set to `2`. By default it is set to `3` to extract both client database
  files and generate maps.
* `-f NUMBER`, `--flat NUMBER`: set to different values to decrease/increase the map size,
  and thus decrease/increase map accuracy.
* `-h`, `--help`: display the usage message, and an example call.


[1]: http://blizzard.com/games/wow/ "World of Warcraft"
