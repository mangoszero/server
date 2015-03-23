Script source code layout
=========================
In order to make it easier to find scripts, we have agreed on a fixed naming scheme
for directories and scripts.

Directories
-----------
* **battlegrounds**: contains scripts used in the Alterac Valley, Arathi Basin and
  Warsong Gulch battlegrounds.
* **eastern_kingdoms**: contains scripts for area triggers, creatures, dungeons,
  instances, etc. related to the Eastern Kingdoms continent. Instances located on
  Eastern Kingdoms are grouped in sub-directories by instance name.
* **kalimdor**: contains scripts for area triggers, creatures, dungeons, instances,
  etc. related to the Kalimdor continent. Instances located on Kalimdor are grouped
  in sub-directories by instance name.
* **world**: contains scripts which are used on every map, and not limited to one
  specific zone. This includes scripts for area triggers, game objects, items, and
  some creatures which can be found over the world. Also, scripts for spells are
  stored here.
Contains scripts for anything that is not related to a specified zone.

Naming Conventions
------------------
Source files should be named `type_objectname.cpp` where

* *type* is replaced by the type of object,
* and *objectname* is replaced by the name of the object, creature, item, or area
  that this script will be used by.

`AddSC` functions used for registering scripts to the server core should use the
form of `void AddSC_filename(void);`.
