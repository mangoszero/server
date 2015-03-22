/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <string>
#include <iostream>

#include "TileAssembler.h"

const char* szVMAPMagic = "VMAP_4.0";

//=======================================================
void Usage(char* prg)
{
    printf("Usage: %s [OPTION]\n\n", prg);
    printf("Assemble vmaps from extracted client model information.\n");
    printf("\n");
    printf("Example:\n");
    printf("- provide source and target path:\n");
    printf("  %s Buildings vmaps\n", prg);
}

int main(int argc, char** argv)
{
    printf("mangos-zero vmap (version %s) assembler\n\n", szVMAPMagic);

    if (argc != 3)
    {
        Usage(argv[0]);
        return 1;
    }

    std::string src = argv[1];
    std::string dest = argv[2];

    std::cout << "using " << src << " as source directory and writing output to " << dest << std::endl;

    VMAP::TileAssembler* ta = new VMAP::TileAssembler(src, dest);

    if (!ta->convertWorld2())
    {
        std::cout << "exit with errors" << std::endl;
        delete ta;
        return 1;
    }

    delete ta;
    std::cout << "Ok, all done" << std::endl;
    return 0;
}
