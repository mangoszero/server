/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_POINTMOVEMENTGENERATOR_H
#define MANGOS_POINTMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

template<class T>
class PointMovementGenerator
    : public MovementGeneratorMedium< T, PointMovementGenerator<T> >
{
    public:
        PointMovementGenerator(uint32 _id, float _x, float _y, float _z, bool _generatePath) :
            id(_id), i_x(_x), i_y(_y), i_z(_z), m_generatePath(_generatePath) {}

        void Initialize(T&);
        void Finalize(T&);
        void Interrupt(T&);
        void Reset(T& unit);
        bool Update(T&, const uint32& diff);

        void MovementInform(T&);

        MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

        bool GetDestination(float& x, float& y, float& z) const { x = i_x; y = i_y; z = i_z; return true; }
    protected:
        uint32 id;
        float i_x, i_y, i_z;
        bool m_generatePath;
};

class AssistanceMovementGenerator
    : public PointMovementGenerator<Creature>
{
    public:
        AssistanceMovementGenerator(float _x, float _y, float _z) :
            PointMovementGenerator<Creature>(0, _x, _y, _z, true) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_MOTION_TYPE; }
        void Initialize(Unit& unit) override;
        void Finalize(Unit&) override;
};

// Does almost nothing - just doesn't allows previous movegen interrupt current effect. Can be reused for charge effect
class EffectMovementGenerator : public MovementGenerator
{
    public:
        explicit EffectMovementGenerator(uint32 Id) : m_Id(Id) {}
        void Initialize(Unit&) override {}
        void Finalize(Unit& unit) override;
        void Interrupt(Unit&) override {}
        void Reset(Unit&) override {}
        bool Update(Unit& u, const uint32&) override;
        MovementGeneratorType GetMovementGeneratorType() const override { return EFFECT_MOTION_TYPE; }
    private:
        uint32 m_Id;
};

class FlyOrLandMovementGenerator : public PointMovementGenerator<Creature>
{
    public:
        FlyOrLandMovementGenerator(uint32 _id, float _x, float _y, float _z, bool liftOff) :
            PointMovementGenerator<Creature>(_id, _x, _y, _z, false),
            m_liftOff(liftOff) {}

        void Initialize(Unit& unit) override;
    private:
        bool m_liftOff;
};

#endif
