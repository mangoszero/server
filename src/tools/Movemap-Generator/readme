mmap generator
==============
The *movement map generator* will extract pathfinding information from the
game client in order to enable the *mangos* server to let creatures walk
on the terrain as naturally as possible.

Please note that due to the complex nature of the process, the extraction and
generation process may require a high amount of computing power any time.

Depending on your system, you should except ranges from four hours up to a full
day for a full map generation of all in-game zones. Reducing the amount of zones
generated will also reduce the time needed to wait.

Requirements
------------
You will need a working installation of the [World of Warcraft][1] client patched
to version 1.12.x.

Usage
-----
The `mmap-generator` command can be used with a set of parameters to fine-tune the
process of movement map generation.

* `--offMeshInput [file.*]`: Path to file containing off mesh connections data,
  with a single mesh connection per line. Format must be:

  `map_id tile_x,tile_y (start_x start_y start_z) (end_x end_y end_z) size  //optional comments`

* `--silent`: Make us script friendly. Do not wait for user input on error or
  completion.
* `--bigBaseUnit [true|false]`: Generate tile/map using bigger basic unit. Use this
  option only if you have unexpected gaps. If set to `false`, we will use normal
  metrics.
* `--maxAngle [#]`: the maximum walkable inclination angle. By default this is set
  to `60`. Float values between 45 and 90 degrees are allowed.
* `--skipLiquid`: skip liquid data for maps. Skipping liquid maps is disabled by
  default.
* `--skipContinents [true|false]`: skip building continents. Disabled by default.
  Available continents are `0` (*Eastern Kingdoms*), and `1` (*Kalimdor*).
* `--skipJunkMaps [true|false]`: skip junk maps, including some unused maps,
  transport maps, and some other maps. Junk maps are skipped by default.
* `--skipBattlegrounds [true|false]`: skip battleground maps. By default battleground
  maps are included.
* `--debugOutput [true|false]`: create debugging files for use with RecastDemo. If you
  are only creating movement maps for use with MaNGOS, you do not need debugging
  files built. By default, debugging files are not created.
* `--tile [#,#]`: Build the specified tile seperate number with a comma ','.
  Must specify a map number (see below). If this option is not used, all tiles are
  built. If only one number is specified, builds the map specified by it.
  This command will build the map regardless of --skip* option settings. If you do
  not specify a map number, builds all maps that pass the filters specified by
  `--skip*` options.
* `-h`, `--help`: show usage information.

Examples
--------

* `mmap-generator`: builds maps using the default settings (see above for defaults)
* `mmap-generator --skipContinents true`: builds the default maps, except continents
* `mmap-generator 0`: builds all tiles of map 0
* `mmap-generator 0 --tile 34,46`: builds only tile 34,46 of map 0 (this is the southern face of blackrock mountain)


[1]: http://blizzard.com/games/wow/ "World of Warcraft"
