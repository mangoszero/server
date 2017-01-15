<table border=0 cellpadding=0 cellspacing=0 valign='top'><tr>
<td><a href='https://www.getmangos.eu' target='getmangos.eu'><img src='https://www.getmangos.eu/!assets_mangos/logo.png' border=0></a></td>
<td valign='top'>
<a href='https://www.getmangos.eu/forums/' target='getmangos.forum'><img src='/icons/FORUM.gif' border=0></a>
<a href='https://www.getmangos.eu/wiki' target='getmangos.wiki'><img src='/icons/WIKI.gif' border=0></a>
<a href='https://www.getmangos.eu/github-activity/' target='getmangos.activity'><img src='/icons/ACTIVITY.gif' border=0></a>
<a href='https://www.getmangos.eu/bug-tracker/mangos-zero/' target='getmangos.tracker'><img src='/icons/TRACKER.gif' border=0></a>
<br />Build Status: <br/>Linux 
<a href='https://travis-ci.org/mangoszero/server/builds' target='MangosZero'><img src='https://travis-ci.org/mangoszero/server.png' border=0></a>
 Windows 
<a href='https://ci.appveyor.com/project/MaNGOS/server-9fytl/history' target='MangosZero'><img src='https://ci.appveyor.com/api/projects/status/github/mangoszero/server?branch=develop21&svg=true' border=0></a>
</td></tr></table>

VANILLA WOW BRANCH
===
### A World of Warcraft server for Vanilla WoW  
----
*Mangos* is open source project, built in [C++][7], it's fast, runs on multiple
platforms, can store game data in [MySQL][40] and [PostgreSQL][42]. It also has
optional support for SOAP, and aims to be 100% compatible with [World of Warcraft][2]
in its vanilla versions, namely [patch 1.12.1][5] and [patch 1.12.2][6].

If you liked the first incarnation of [World of Warcraft][2] and still want to play
[vanilla WoW][4], this is the branch for you. We provide an authentication
server where you can manage your users, and a world server which serves game
content just like the original did back then.

On top of that each update is built by [Travis CI][16] as you can see by the
image next to the chapter's heading! We do love green builds, and working things.
To complement this, we push builds through [Coverity][17] to find and fix any
possible security issues.

World of Warcraft, and all World of Warcraft or Warcraft art, images, and lore are
copyrighted by [Blizzard Entertainment, Inc.][1]

Requirements
------------
The *Mangos* server supports a wide range of operating systems, and various
compiler platforms. In order to do that, we use various free cross-platform
libraries and use [CMake][19] to provide a cross-platform build system which
adapts to your chosen operating system and compiler.

Operating systems
-----------------
Currently we support running *Mangos* on these operating systems:

* **Windows**, 32 bit and 64 bit. [Windows][20] Server 2008 (or newer) or Windows 7 (or newer) is recommended.
* **Linux**, 32 bit and 64 bit. [Debian 7][21] and [Ubuntu 12.04 LTS][22] are
  recommended. Other distributions with similar package versions will work, too.
* **BSD**, 32 bit and 64 bit. [FreeBSD][23], [NetBSD][24], [OpenBSD][25] and
  [DragonFly][26] are recommended.

Of course, newer versions should work, too. In the case of Windows, matching
server versions will work, too.

Compilers
---------
Building *Mangos* is currently possible with these compilers:

* **Microsoft Visual Studio (Express[^1])**, 32 bit and 64 bit. Both
  [Visual Studio][30] and the Express editions of [Visual Studio][31] are
  supported. Only Visual Studio 2012 and above are now officially supported.

* **Microsoft Windows SDK**, 32 bit and 64 bit. The [Windows 7 SDK][32] is
  recommended, as older versions lack compiler features required to build
  the server.
* **Clang**, 32 bit and 64 bit. The [Clang compiler][33] can be used on any
  supported operating system.[^2]

Dependencies
------------
The *Mangos* server stands on the shoulders of well-known Open Source
libraries, and a few awesome, but less known libraries to prevent us from
inventing the wheel again.

*Please note that Linux and Mac OS X users should install packages using
their systems package management instead of source packages.*

* **MySQL** / **PostgreSQL**: to store content, and user data, we rely on
  [MySQL][40]/[MariaDB][41] and [PostgreSQL][42] to handle data.
* **ACE**: the [ADAPTIVE Communication Environment][43] aka. *ACE* provides us
  with a solid cross-platform framework for abstracting operating system
  specific details.
* **Recast**: in order to create navigation data from the client's map files,
  we use [Recast][44] to do the dirty work. It provides functions for
  rendering, pathing, etc.
* **G3D**: the [G3D][45] engine provides the basic framework for handling 3D
  data, and is used to handle basic map data.
* **libmpq**: [libmpq][46] provides an abstraction layer for reading from the
  client's data files.
* **Zlib**: [Zlib][53] ([Zlib for Windows][51]) provides compression algorithms
  used in both MPQ archive handling and the client/server protocol.
* **Bzip2**: [Bzip2][54] ([Bzip2 for Windows][52]) provides compression
  algorithms used in MPQ archives.
* **OpenSSL**: [OpenSSL][48] ([OpenSSL for Windows][55]) provides encryption
  algorithms used when authenticating clients.
* **Lua**: [Lua 5.2][56] ([Lua 5.2 for Windows][57]) provides a convenient, fast
  scripting environment, which allows us to make live changes to scripted
  content.

*ACE*, *Recast*, *G3D* and *libmpq* are included in the *Mangos* distribution as
we rely on specific versions.

Optional dependencies
---------------------

* **Doxygen**: if you want to export HTML or PDF formatted documentation for the
  *Mangos* API, you should install [Doxygen][49].
* **Lua**: if you want to test drive how *Mangos* feels with [Lua][50] as
  scripting language instead of the scripting library, you will need Lua **5.2.3**
  installed. Although an internal version is included.

Discuss
-------
If you need help with building and installing *Mangos* there are thousands of
users out there already running *Mangos* and many more you can find on our
project website and discussion forum to assist with any issues you may have.

* [getmangos.eu][10]

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

The full license is included in the file `License.md`.

In addition, as a special exception, permission is granted to link the code of
*Mangos* with the OpenSSL project's [OpenSSL library][48] (or with modified
versions of it that use the same license as the OpenSSL library), and distribute
the linked executables. You must obey the GNU General Public License in all
respects for all of the code used other than [OpenSSL][48].


[^1]: Visual Studio Express versions prior to the 2012 release can only
      build 32 bit applications, unless you install the Windows SDK.
[^2]: Clang support for Windows is experimental. Failure to compile Mangos
      may also relate to the experimental state of the port.

[1]: http://blizzard.com/ "Blizzard Entertainment Inc. · we love you!"
[2]: http://blizzard.com/games/wow/ "World of Warcraft · Classic / Vanilla"
[3]: http://wowpedia.org/Beta#World_of_Warcraft "World of Warcraft - Classic Beta"
[4]: http://www.wowpedia.org/Patch_1.12.0 "Vanilla WoW · Patch 1.12.0 release notes"
[5]: http://www.wowpedia.org/Patch_1.12.1 "Vanilla WoW · Patch 1.12.1 release notes"
[6]: http://www.wowpedia.org/Patch_1.12.2 "Vanilla WoW · Patch 1.12.2 release notes"
[7]: http://www.cppreference.com/ "C / C++ reference"

[10]: https://getmangos.eu/ "mangos · project site"
[12]: http://github.com/mangoszero "MaNGOS Zero · github organization"
[13]: http://github.com/mangoszero/server "MaNGOS Zero · server repository"
[15]: http://github.com/mangoszero/database "MaNGOS Zero · content database repository"
[16]: https://travis-ci.org/mangoszero/server "Travis CI . MaNGOS Zero build status"
[17]: https://scan.coverity.com/ "Coverity Scan · Static Code Analysis"

[19]: http://www.cmake.org/ "CMake · Cross Platform Make"
[20]: http://windows.microsoft.com/ "Microsoft Windows · that OS, yes."
[21]: http://www.debian.org/ "Debian · The Universal Operating System"
[22]: http://www.ubuntu.com/ "Ubuntu · The world's most popular free OS"
[23]: http://www.freebsd.org/ "FreeBSD · The Power To Serve"
[24]: http://www.netbsd.org/ "NetBSD · The NetBSD Project"
[25]: http://www.openbsd.org/ "OpenBSD · Free, functional and secure"
[26]: http://www.dragonflybsd.org/ "DragonFlyBSD"

[30]: http://www.microsoft.com/visualstudio/eng/ "Visual Studio 2012"
[31]: http://www.microsoft.com/visualstudio/eng/products/visual-studio-express-products "Visual Studio Express 2012 for Windows Desktop"
[32]: http://www.microsoft.com/en-us/download/details.aspx?id=8279 "Windows SDK for Windows 7 and .NET Framework 4"
[33]: http://clang.llvm.org/ "clang · a C language family frontend for LLVM"
[34]: http://git-scm.com/ "Git · Distributed version control system"
[35]: http://windows.github.com/ "github · windows client"
[36]: http://www.sourcetreeapp.com/ "SourceTree · Free Mercurial and Git Client for Windows/Mac"

[40]: http://www.mysql.com/ "MySQL · The world's most popular open source database"
[41]: http://www.mariadb.org/ "MariaDB · An enhanced, drop-in replacement for MySQL"
[42]: http://www.postgresql.org/ "PostgreSQL · The world's most advanced open source database"
[43]: http://www.cs.wustl.edu/~schmidt/ACE.html "ACE · The ADAPTIVE Communication Environment"
[44]: http://github.com/memononen/recastnavigation "Recast · Navigation-mesh Toolset for Games"
[45]: http://sourceforge.net/projects/g3d/ "G3D · G3D Innovation Engine"
[46]: http://github.com/ge0rg/libmpq "libmpq · A library for reading data from MPQ archives"
[48]: http://www.openssl.org/ "OpenSSL · The Open Source toolkit for SSL/TLS"
[49]: http://www.stack.nl/~dimitri/doxygen/ "Doxygen · API documentation generator"
[50]: http://www.lua.org/ "Lua · The Programming Language"
[51]: http://gnuwin32.sourceforge.net/packages/zlib.htm "Zlib for Windows"
[52]: http://gnuwin32.sourceforge.net/packages/bzip2.htm "Bzip2 for Windows"
[53]: http://www.zlib.net/ "Zlib"
[54]: http://www.bzip.org/ "Bzip2"
[55]: http://slproweb.com/products/Win32OpenSSL.html "OpenSSL for Windows"
[56]: http://www.lua.org/ "Lua"
[57]: https://code.google.com/p/luaforwindows/ "Lua for Windows"

