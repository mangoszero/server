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

#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include "GameObject.h"

#include <map>
#include <set>
#include <string>

class Map;
class TransportMap;

/**
 * @brief A vessel: a game object that is a CLOCK and a RELAY, and holds the map it IS.
 *
 * It has exactly three jobs, and no fourth:
 *
 *  1. Run the route clock, whose only decision is WHEN the ship changes world map, and hand
 *     the client the path progress it needs -- that number and her entry are the whole of
 *     what the client is told. It draws the hull itself, out of an animation the server does
 *     not have; her create block carries the position (0, 0, 0) and always will.
 *  2. Be the relay between the world and her own map, in both directions.
 *  3. Tick her own map, LAST, nested inside her own tick.
 *
 * Everything about who is aboard -- crew, passengers, pets, where they stand, what the shore
 * is told about them -- belongs to the map and is asked of the map (`AsMap()`), because a
 * ship is not a container of people, it is a place.
 *
 * The waypoint pose is not a position and is never treated as one. Nothing is redrawn from
 * it, nothing is repositioned by it, and nothing is ever composed with it. It exists for one
 * purpose: to name the WORLD GRID to look in when the relay goes looking for what is ashore.
 */
class Transport : public GameObject
{
    public:
        explicit Transport();

        bool Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint8 animprogress);
        bool GenerateWaypoints(uint32 pathid, std::set<uint32>& mapids);
        void Update(uint32 update_diff, uint32 p_time) override;

        /**
         * @brief IS SHE BETWEEN TWO WORLD MAPS?
         *
         * Crossing is decided on the map she is LEAVING, on that map's thread, and completed
         * past MapManager's barrier where no map is running at all. It has to be: arriving
         * writes into the destination's containers, and the destination may be updating on
         * another thread at the very moment the route says go.
         */
        bool IsCrossing() const { return m_crossing; }

        /// Finish it. MapManager only, and only past the barrier.
        void CompleteCrossing();

        /// Take the vessel out of the world it sails: off the active list, out of the map's
        /// store, map pointer dropped. Called while the maps are still alive -- nothing else
        /// does it, because a vessel is in no cell and no grid unload reaches it.
        void WithdrawFromWorld();

        /// THE MAP THIS SHIP IS. NULL when the baker left no hull for her, in which case she
        /// is an ordinary game object that carries nobody.
        TransportMap* AsMap() const { return m_map; }

        /**
         * @brief The Map.dbc id of a vessel's hull, given once at start-up.
         *
         * Blizzard ships a row per vessel whose directory is "Transport<goEntry>" and no
         * terrain for it -- the hull only ever existed as a game object model, which the
         * baker turns into that map's terrain. Several legitimate ships have no row at all,
         * and some rows are named for a route, which names no vessel we can key on; those are
         * minted one here and a Map.dbc entry injected to carry it. Nothing on the wire ever
         * carries this number.
         */
        static void RegisterVesselMap(uint32 goEntry, char const* vesselName);
        static uint32 VesselMapIdOf(uint32 goEntry);

        /// Is this map some vessel's hull? Such maps are ticked by the vessel that owns them
        /// and must never be scheduled or unloaded with the rest.
        static bool IsVesselMapId(uint32 mapId);

        uint32 VesselMapId() const { return VesselMapIdOf(GetEntry()); }

        /// Pin every world grid the route passes through, at start-up and for good. The
        /// vessel is an active object in those grids: they must be there before she sails
        /// into them, because what the relay finds ashore is whatever those cells hold.
        void PinRouteGrids();

        /// The vessel `obj` is aboard, or NULL. DERIVED, NEVER STORED: being aboard is what
        /// having that map MEANS.
        static Transport* VesselOf(WorldObject const& obj);

        /// The vessel of the given GUID on `map`, or NULL. For resolving a transport a client
        /// packet named by GUID when we do not already hold the object.
        static Transport* GetTransport(Map const* map, ObjectGuid guid);

        /// How far along her route she is, in milliseconds of path time. This is the one
        /// number the client needs to place the hull: it interpolates her from this and the
        /// path, and we estimate her from the same value, so both sides agree.
        uint32 GetPathProgress() const { return m_timer; }

        /// Half the longest gap between consecutive waypoints: how far the estimate can be
        /// off, since it snaps between nodes and never interpolates. The relay widens its
        /// grid search by it, and nothing else ever asks.
        float NodeSlack() const { return m_nodeSlack; }

    private:
        struct WayPoint
        {
            WayPoint() : mapid(0), x(0), y(0), z(0), teleport(false) {}
            WayPoint(uint32 _mapid, float _x, float _y, float _z, bool _teleport)
                : mapid(_mapid), x(_x), y(_y), z(_z), teleport(_teleport)
            {
            }

            uint32 mapid;
            float x;
            float y;
            float z;
            bool teleport;
        };

        typedef std::map<uint32, WayPoint> WayPointMap;

        WayPointMap::const_iterator m_curr;
        WayPointMap::const_iterator m_next;
        uint32 m_pathTime;
        uint32 m_timer;

        float m_nodeSlack = 0.0f;

        bool m_withdrawn = false;

        /// The world map she is on her way to, and nothing yet done about it. The point is
        /// the TAXI NODE's own, copied out of the route the moment it was read: what the
        /// passengers are sent to must be the number the client's own path is built from,
        /// not a pose we recomputed on the way past it.
        uint32 m_crossingTo = 0;
        float m_crossingX = 0.0f;
        float m_crossingY = 0.0f;
        float m_crossingZ = 0.0f;
        bool m_crossing = false;

        /// THE ONE REFERENCE. Owned by MapManager like any map; the vessel only holds it.
        TransportMap* m_map = NULL;

    public:
        WayPointMap m_WayPoints;
        uint32 m_nextNodeTime;
        uint32 m_period;

    private:
        void TeleportTransport(uint32 newMapid, float x, float y, float z);
        void MoveToNextWayPoint();                          // move m_next/m_cur to next points
};
#endif
