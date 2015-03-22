vmap extractor
--------------
The *vmap extractor* will extract model information from the game client.

Requirements
------------
You will need a working installation of the [World of Warcraft][1] client patched
to version 1.12.x.

Instructions - Linux
--------------------
Use the created executable to extract model information. Change the data path if
needed.

    $ vmap-extractor -d /mnt/windows/games/world of warcraft/

Resulting files will be in `./Buildings`.

Instructions - Windows
----------------------
Use the created executable (from command prompt) to extract model information.
It should find the data path for your client installation through the Windows
registry, but the data path can be specified with the -d option.

Resulting files will be in `.\Buildings`.

Parameters
----------
The *vmap extractor* can be used with a few parameters to customize input, output
and generated output.

* `-d PATH`, `--data PATH`: set the path for reading the client's MPQ archives to the given
  path.
* `-s`, `--small`: small size (data size optimization), ~500MB less vmap data. This is the
  default setting.
* `-l`, `--large`: large size, ~500MB more vmap data. Stores additional details in vmap data.
* `-h`, `--help`: display the usage message, and an example call.


[1]: http://blizzard.com/games/wow/ "World of Warcraft"
