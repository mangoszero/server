/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOSSERVER_MOVESPLINEFLAG_H
#define MANGOSSERVER_MOVESPLINEFLAG_H

#include "typedefs.h"
namespace Movement
{
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

    /**
     * @brief Class representing movement spline flags.
     */
    class MoveSplineFlag
    {
        public:
            /**
             * @brief Enum for movement spline flags.
             */
            enum eFlags
            {
                None         = 0x00000000,
                Done         = 0x00000001,
                Falling      = 0x00000002,           // Affects elevation computation
                Unknown3     = 0x00000004,
                Unknown4     = 0x00000008,
                Unknown5     = 0x00000010,
                Unknown6     = 0x00000020,
                Unknown7     = 0x00000040,
                Unknown8     = 0x00000080,
                Runmode      = 0x00000100,
                Flying       = 0x00000200,           // Smooth movement (Catmullrom interpolation mode), flying animation
                No_Spline    = 0x00000400,
                Unknown12    = 0x00000800,
                Unknown13    = 0x00001000,
                Unknown14    = 0x00002000,
                Unknown15    = 0x00004000,
                Unknown16    = 0x00008000,
                Final_Point  = 0x00010000,
                Final_Target = 0x00020000,
                Final_Angle  = 0x00040000,
                Unknown19    = 0x00080000,           // Exists, but unknown what it does
                Cyclic       = 0x00100000,           // Movement by cycled spline
                Enter_Cycle  = 0x00200000,           // Appears with cyclic flag in monster move packet, erases first spline vertex after first cycle done
                Frozen       = 0x00400000,           // Will never arrive
                Unknown23    = 0x00800000,
                Unknown24    = 0x01000000,
                Unknown25    = 0x02000000,           // Exists, but unknown what it does
                Unknown26    = 0x04000000,
                Unknown27    = 0x08000000,
                Unknown28    = 0x10000000,
                Unknown29    = 0x20000000,
                Unknown30    = 0x40000000,
                Unknown31    = 0x80000000,

                // Masks
                Mask_Final_Facing = Final_Point | Final_Target | Final_Angle,
                // Flags that shouldn't be appended into SMSG_MONSTER_MOVE\SMSG_MONSTER_MOVE_TRANSPORT packet
                Mask_No_Monster_Move = Mask_Final_Facing | Done,
                // CatmullRom interpolation mode used
                Mask_CatmullRom = Flying
            };

            /**
             * @brief Gets the raw flag value.
             * @return uint32& Reference to the raw flag value.
             */
            inline uint32& raw() { return (uint32&) * this;}
            /**
             * @brief Gets the raw flag value.
             * @return const uint32& Reference to the raw flag value.
             */
            inline const uint32& raw() const { return (const uint32&) * this;}

            /**
             * @brief Default constructor for MoveSplineFlag.
             */
            MoveSplineFlag() { raw() = 0; }
            /**
             * @brief Constructor for MoveSplineFlag with a flag value.
             * @param f The flag value.
             */
            MoveSplineFlag(uint32 f) { raw() = f; }
            /**
             * @brief Copy constructor for MoveSplineFlag.
             * @param f The MoveSplineFlag to copy.
             */
            MoveSplineFlag(const MoveSplineFlag& f) { raw() = f.raw(); }

            // Constant interface

            /**
             * @brief Checks if the movement is smooth.
             * @return bool True if the movement is smooth, false otherwise.
             */
            bool isSmooth() const { return raw() & Mask_CatmullRom;}
            /**
             * @brief Checks if the movement has a final facing.
             * @return bool True if the movement has a final facing, false otherwise.
             */
            bool isFacing() const { return raw() & Mask_Final_Facing;}

            /**
             * @brief Checks if all specified flags are set.
             * @param f The flags to check.
             * @return bool True if all specified flags are set, false otherwise.
             */
            bool hasAllFlags(uint32 f) const { return (raw() & f) == f;}
            /**
             * @brief Bitwise AND operator for flags.
             * @param f The flags to AND.
             * @return uint32 The result of the AND operation.
             */
            uint32 operator & (uint32 f) const { return (raw() & f);}
            /**
             * @brief Bitwise OR operator for flags.
             * @param f The flags to OR.
             * @return uint32 The result of the OR operation.
             */
            uint32 operator | (uint32 f) const { return (raw() | f);}
            /**
             * @brief Converts the flags to a string representation.
             * @return std::string The string representation of the flags.
             */
            std::string ToString() const;

            // Not constant interface

            /**
             * @brief Bitwise AND assignment operator for flags.
             * @param f The flags to AND.
             */
            void operator &= (uint32 f) { raw() &= f;}
            /**
             * @brief Bitwise OR assignment operator for flags.
             * @param f The flags to OR.
             */
            void operator |= (uint32 f) { raw() |= f;}

            /**
             * @brief Enables facing a point.
             */
            void EnableFacingPoint()    { raw() = (raw() & ~Mask_Final_Facing) | Final_Point;}
            /**
             * @brief Enables facing an angle.
             */
            void EnableFacingAngle()    { raw() = (raw() & ~Mask_Final_Facing) | Final_Angle;}
            /**
             * @brief Enables facing a target.
             */
            void EnableFacingTarget()   { raw() = (raw() & ~Mask_Final_Facing) | Final_Target;}

            bool done          : 1; /**< Flag indicating if the movement is done. */
            bool falling       : 1; /**< Flag indicating if the movement is falling. */
            bool unknown3      : 1; /**< Unknown flag. */
            bool unknown4      : 1; /**< Unknown flag. */
            bool unknown5      : 1; /**< Unknown flag. */
            bool unknown6      : 1; /**< Unknown flag. */
            bool unknown7      : 1; /**< Unknown flag. */
            bool unknown8      : 1; /**< Unknown flag. */
            bool runmode       : 1; /**< Flag indicating if the movement is in run mode. */
            bool flying        : 1; /**< Flag indicating if the movement is flying. */
            bool no_spline     : 1; /**< Flag indicating if there is no spline. */
            bool unknown12     : 1; /**< Unknown flag. */
            bool unknown13     : 1; /**< Unknown flag. */
            bool unknown14     : 1; /**< Unknown flag. */
            bool unknown15     : 1; /**< Unknown flag. */
            bool unknown16     : 1; /**< Unknown flag. */
            bool final_point   : 1; /**< Flag indicating if the movement has a final point. */
            bool final_target  : 1; /**< Flag indicating if the movement has a final target. */
            bool final_angle   : 1; /**< Flag indicating if the movement has a final angle. */
            bool unknown19     : 1; /**< Unknown flag. */
            bool cyclic        : 1; /**< Flag indicating if the movement is cyclic. */
            bool enter_cycle   : 1; /**< Flag indicating if the movement enters a cycle. */
            bool frozen        : 1; /**< Flag indicating if the movement is frozen. */
            bool unknown23     : 1; /**< Unknown flag. */
            bool unknown24     : 1; /**< Unknown flag. */
            bool unknown25     : 1; /**< Unknown flag. */
            bool unknown26     : 1; /**< Unknown flag. */
            bool unknown27     : 1; /**< Unknown flag. */
            bool unknown28     : 1; /**< Unknown flag. */
            bool unknown29     : 1; /**< Unknown flag. */
            bool unknown30     : 1; /**< Unknown flag. */
            bool unknown31     : 1; /**< Unknown flag. */
    };
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif
}

#endif // MANGOSSERVER_MOVESPLINEFLAG_H

