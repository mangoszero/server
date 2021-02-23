[<img src='https://www.getmangos.eu/!assets_mangos/currentlogo.gif' width="48" border=0>](https://www.getmangos.eu)
[<img src='https://www.getmangos.eu/!assets_mangos/logo.png' border=0>](https://www.getmangos.eu)

Build Status:<br><b>Linux/MAC:</b>
[<img src='https://travis-ci.org/mangoszero/server.png' border=0 valign="middle">](https://travis-ci.org/mangoszero/server/builds)
<b>Windows:</b>
[<img src='https://ci.appveyor.com/api/projects/status/github/mangoszero/server?branch=master&svg=true' border=0 valign="middle">](https://ci.appveyor.com/project/MaNGOS/server-9fytl/history)
 <b>Codacy Status:</b> 
[<img src='https://api.codacy.com/project/badge/Grade/895a7434531a456ba12410ac585717c8' border=0 valign="middle"/>](https://app.codacy.com/gh/mangoszero/server/dashboard)

---

[<img src="https://www.getmangos.eu/!assets_mangos/Mangos0.png" width="48" valign="middle"/>](http://getmangos.eu)
 **MangosZero - Vanilla WoW server**
===

**Mangos** is an open source project written in [C++][7]. It's fast, runs on multiple
platforms and stores game data in [MySQL][40] or [MariaDB][41]. It also has 
optional support for SOAP.

If you liked the original incarnation of [World of Warcraft][2] and still want to play it,
this is the branch for you. We provide an authentication server where you can manage your users, 
and a world server which serves game content just like the original did back then.

It aims to be 100% compatible with the 3 final versions of Vanilla [World of Warcraft][2], 
namely [patch 1.12.1][4], [patch 1.12.2][5] & [patch 1.12.3][6].
<br>**IT DOES NOT SUPPORT 1.13.x** and beyond which is the newly released Classic Experience (NuClassic).

On top of that each update is automatically built by [Travis CI][16] (Linux/MAC) and [AppVeyor][17] (Windows)
as you can see by the images in the heading above! We do love green builds, and working things.

Requirements
------------
The server supports a wide range of operating systems, and various compiler platforms.
In order to do that, we use various free cross-platform libraries and use [CMake][19] to provide
a cross-platform build system which adapts to your chosen operating system and compiler.

Operating systems
-----------------
Currently we support running the server on the following operating systems:

* **Windows**, 32 bit and 64 bit. [Windows][20] Server 2008 (or newer) or Windows 7 (or newer) is recommended.
* **Linux**, 32 bit and 64 bit. [Debian 7][21] and [Ubuntu 12.04 LTS][22] are
  recommended. Other distributions with similar package versions will work, too.
* **BSD**, 32 bit and 64 bit. [FreeBSD][23], [NetBSD][24], [OpenBSD][25] are recommended.

Of course, newer versions should work, too. In the case of Windows, matching
server versions will work, too.

Compilers
---------
Building the server is currently possible with these compilers:

* **Microsoft Visual Studio 32 bit and 64 bit.** All editions of [Visual Studio][31]
from 2015 upwards are officially supported.

* **Clang**, 32 bit and 64 bit. The [Clang compiler][33] can be used on any
  supported operating system.

Dependencies
------------
The server stands on the shoulders of several well-known Open Source libraries plus
a few awesome, but less known libraries to prevent us from inventing the wheel again.

**Please note that Linux and Mac OS X users should install packages using
their systems package management instead of source packages.**

* **MySQL** / **MariaDB**: to store content, and user data, we rely on
  [MySQL][40]/[MariaDB][41] to handle data.
* **ACE**: the [ADAPTIVE Communication Environment][43] aka. *ACE* provides us
  with a solid cross-platform framework for abstracting operating system
  specific details.
* **Recast**: in order to create navigation data from the client's map files,
  we use [Recast][44] to do the dirty work. It provides functions for
  rendering, pathing, etc.
* **G3D**: the [G3D][45] engine provides the basic framework for handling 3D
  data, and is used to handle basic map data.
* **Stormlib**: [Stormlib][46] provides an abstraction layer for reading from the
  client's data files.
* **Zlib**: [Zlib][53] ([Zlib for Windows][51]) provides compression algorithms
  used in both MPQ archive handling and the client/server protocol.
* **Bzip2**: [Bzip2][54] ([Bzip2 for Windows][52]) provides compression
  algorithms used in MPQ archives.
* **OpenSSL**: [OpenSSL][48] ([OpenSSL for Windows][55]) provides encryption
  algorithms used when authenticating clients.

**ACE**, **Recast**, **G3D**, **Stormlib**, **Zlib** and **Bzip2** are included in the standard distribution as
we rely on specific versions.

Optional dependencies
---------------------

* **Doxygen**: if you want to export HTML or PDF formatted documentation for the
  Mangos API, you should install [Doxygen][49].


<br>We have a small, but extremely friendly and helpful community managed by MadMax and Antz.
<br>Any trolling or unpleasantness is swiftly dealt with !!
- Our discord/forum motto is "Be nice or Be somewhere else"

**Official Website**
----

We welcome anyone who is interested in enjoying older versions of wow or contributing and helping out !

* [**Official MaNGOS Website**](https://getmangos.eu/)  

**Discord Server**
----
[![Widget for the Discord API guild](https://discord.com/api/guilds/286167585270005763/widget.png?style=banner2)](https://discord.gg/CzXcBXq) 

**Main Wiki**
----

The repository of as much information as we can pack in. Details regarding the Database, file type definitions, packet definitons etc.

* [**Wiki Table of Contents**](http://getmangos.eu/wiki)  


**Bug / Issue Tracker**
----

Found an issue or something which doesn't seem right, please log it in the relevant section of the Bug Tracker.

* [**Bug Tracker**](https://www.getmangos.eu/bug-tracker/mangos-zero/)

**Installation Guides**
----

Installation instructions for various operation systems can be found here.

* [**Installation Guides**](https://www.getmangos.eu/wiki/documentation/installation-guides/) 


License
-------
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA 02110-1301 USA.

The full license is included in the file [LICENSE](LICENSE).

We have all put in hundreds of hours of time for free to make the server what it
is today.
<br>All we ask is that if you modify the code and make improvements, please have
the decency to feed those changes back to us.

In addition, as a special exception, permission is granted to link the code of
*Mangos* with the OpenSSL project's [OpenSSL library][48] (or with modified
versions of it that use the same license as the OpenSSL library), and distribute
the linked executables. You must obey the GNU General Public License in all
respects for all of the code used other than [OpenSSL][48].

Acknowledgements
--------
World of Warcraft, and all related art, images, and lore are copyright [Blizzard Entertainment, Inc.][1]


[1]: http://blizzard.com/ "Blizzard Entertainment Inc. · we love you!"
[2]: https://worldofwarcraft.com/ "World of Warcraft
[4]: http://www.wowpedia.org/Patch_1.12.1 "Vanilla WoW · Patch 1.12.1 release notes"
[5]: http://www.wowpedia.org/Patch_1.12.2 "Vanilla WoW · Patch 1.12.2 release notes"
[6]: http://www.wowpedia.org/Patch_1.12.3 "Vanilla WoW · Patch 1.12.3 release notes"
[7]: http://www.cppreference.com/ "C / C++ reference"

[10]: https://getmangos.eu/ "mangos · project site"
[12]: https://github.com/mangoszero "MaNGOS Zero · github organization"
[13]: https://github.com/mangoszero/server "MaNGOS Zero · server repository"
[15]: https://github.com/mangoszero/database "MaNGOS Zero · content database repository"
[16]: https://travis-ci.org/mangoszero/server "Travis CI · Linux/MAC build status"
[17]: https://ci.appveyor.com/ "AppVeyor Scan · Windows build status"
[19]: http://www.cmake.org/ "CMake · Cross Platform Make"
[20]: http://windows.microsoft.com/ "Microsoft Windows"
[21]: http://www.debian.org/ "Debian · The Universal Operating System"
[22]: http://www.ubuntu.com/ "Ubuntu · The world's most popular free OS"
[23]: http://www.freebsd.org/ "FreeBSD · The Power To Serve"
[24]: http://www.netbsd.org/ "NetBSD · The NetBSD Project"
[25]: http://www.openbsd.org/ "OpenBSD · Free, functional and secure"
[31]: https://visualstudio.microsoft.com/vs/older-downloads/ "Visual Studio Downloads"
[33]: http://clang.llvm.org/ "clang · a C language family frontend for LLVM"
[34]: http://git-scm.com/ "Git · Distributed version control system"
[35]: http://windows.github.com/ "github · windows client"
[36]: http://www.sourcetreeapp.com/ "SourceTree · Free Mercurial and Git Client for Windows/Mac"

[40]: http://www.mysql.com/ "MySQL · The world's most popular open source database"
[41]: http://www.mariadb.org/ "MariaDB · An enhanced, drop-in replacement for MySQL"
[43]: http://www.dre.vanderbilt.edu/~schmidt/ACE.html "ACE · The ADAPTIVE Communication Environment"
[44]: http://github.com/memononen/recastnavigation "Recast · Navigation-mesh Toolset for Games"
[45]: http://sourceforge.net/projects/g3d/ "G3D · G3D Innovation Engine"
[46]: http://zezula.net/en/mpq/stormlib.html "Stormlib · A library for reading data from MPQ archives"
[48]: http://www.openssl.org/ "OpenSSL · The Open Source toolkit for SSL/TLS"
[49]: http://www.stack.nl/~dimitri/doxygen/ "Doxygen · API documentation generator"
[51]: http://gnuwin32.sourceforge.net/packages/zlib.htm "Zlib for Windows"
[52]: http://gnuwin32.sourceforge.net/packages/bzip2.htm "Bzip2 for Windows"
[53]: http://www.zlib.net/ "Zlib"
[54]: http://www.bzip.org/ "Bzip2"
[55]: http://slproweb.com/products/Win32OpenSSL.html "OpenSSL for Windows"
