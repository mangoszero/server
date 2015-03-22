vmap assembler
--------------
The *vmap assembler* will assemble custom height maps based on the model information
extracted with the *vmap extractor*.

Requirements
------------
You will need a working installation of the [World of Warcraft][1] client patched
to version 1.12.x.

Instructions - Linux
--------------------
Use the created executable to create the vmap files for MaNGOS.

The executable takes two arguments:

    vmap-assembler <input_dir> <output_dir>

Example:

    $ ./vmap-assembler Buildings vmaps

<output_dir> has to exist already and shall be empty.

The resulting files in <output_dir> are expected to be found in ${DataDir}/vmaps
by mangos-worldd (DataDir is set in mangosd.conf).

Instructions - Windows
----------------------
Use the created executable (from command prompt) to create the vmap files for MaNGOS.
The executable takes two arguments:

    vmap-assembler.exe <input_dir> <output_dir>

Example:

    C:\my_data_dir\> vmap-assembler.exe Buildings vmaps

<output_dir> has to exist already and shall be empty.
The resulting files in <output_dir> are expected to be found in ${DataDir}\vmaps
by mangos-worldd (DataDir is set in mangosd.conf).


[1]: http://blizzard.com/games/wow/ "World of Warcraft"
