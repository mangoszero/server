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

#ifndef MANGOS_GRIDNOTIFIERS_H
#define MANGOS_GRIDNOTIFIERS_H

#include "UpdateData.h"

#include "Corpse.h"
#include "Object.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Player.h"
#include "Unit.h"

namespace MaNGOS
{
    struct VisibleNotifier
    {
        Camera& i_camera;
        UpdateData i_data;
        GuidSet i_clientGUIDs;
        std::set<WorldObject*> i_visibleNow;

        explicit VisibleNotifier(Camera& c) : i_camera(c), i_clientGUIDs(c.GetOwner()->m_clientGUIDs) {}
        template<class T> void Visit(GridRefManager<T>& m);
        void Visit(CameraMapType& /*m*/) {}
        void Notify(void);
    };

    struct VisibleChangesNotifier
    {
        WorldObject& i_object;

        explicit VisibleChangesNotifier(WorldObject& object) : i_object(object) {}
        template<class T> void Visit(GridRefManager<T>&) {}
        void Visit(CameraMapType&);
    };

    struct MessageDeliverer
    {
        Player const& i_player;
        WorldPacket* i_message;
        bool i_toSelf;
        MessageDeliverer(Player const& pl, WorldPacket* msg, bool to_self) : i_player(pl), i_message(msg), i_toSelf(to_self) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct MessageDelivererExcept
    {
        WorldPacket*  i_message;
        Player const* i_skipped_receiver;

        MessageDelivererExcept(WorldPacket* msg, Player const* skipped)
            : i_message(msg), i_skipped_receiver(skipped) {}

        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct ObjectMessageDeliverer
    {
        WorldPacket* i_message;
        explicit ObjectMessageDeliverer(WorldPacket* msg) : i_message(msg) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct MessageDistDeliverer
    {
        Player const& i_player;
        WorldPacket* i_message;
        bool i_toSelf;
        bool i_ownTeamOnly;
        float i_dist;

        MessageDistDeliverer(Player const& pl, WorldPacket* msg, float dist, bool to_self, bool ownTeamOnly)
            : i_player(pl), i_message(msg), i_toSelf(to_self), i_ownTeamOnly(ownTeamOnly), i_dist(dist) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct ObjectMessageDistDeliverer
    {
        WorldObject const& i_object;
        WorldPacket* i_message;
        float i_dist;
        ObjectMessageDistDeliverer(WorldObject const& obj, WorldPacket* msg, float dist) : i_object(obj), i_message(msg), i_dist(dist) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    struct ObjectUpdater
    {
        uint32 i_timeDiff;
        explicit ObjectUpdater(const uint32& diff) : i_timeDiff(diff) {}
        template<class T> void Visit(GridRefManager<T>& m);
        void Visit(PlayerMapType&) {}
        void Visit(CorpseMapType&) {}
        void Visit(CameraMapType&) {}
        void Visit(CreatureMapType&);
    };

    struct PlayerRelocationNotifier
    {
        Player& i_player;
        PlayerRelocationNotifier(Player& pl) : i_player(pl) {}
        template<class T> void Visit(GridRefManager<T>&) {}
        void Visit(CreatureMapType&);
    };

    struct CreatureRelocationNotifier
    {
        Creature& i_creature;
        CreatureRelocationNotifier(Creature& c) : i_creature(c) {}
        template<class T> void Visit(GridRefManager<T>&) {}
#ifdef WIN32
        template<> void Visit(PlayerMapType&);
#endif
    };

    struct DynamicObjectUpdater
    {
        DynamicObject& i_dynobject;
        Unit* i_check;
        bool i_positive;
        DynamicObjectUpdater(DynamicObject& dynobject, Unit* caster, bool positive) : i_dynobject(dynobject), i_positive(positive)
        {
            i_check = caster;
            Unit* owner = i_check->GetOwner();
            if (owner)
                { i_check = owner; }
        }

        template<class T> inline void Visit(GridRefManager<T>&) {}
#ifdef WIN32
        template<> inline void Visit<Player>(PlayerMapType&);
        template<> inline void Visit<Creature>(CreatureMapType&);
#endif

        void VisitHelper(Unit* target);
    };

    // SEARCHERS & LIST SEARCHERS & WORKERS

    /* Model Searcher class:
    template<class Check>
    struct SomeSearcher
    {
        ResultType& i_result;
        Check & i_check;

        SomeSearcher(ResultType& result, Check & check)
            : i_phaseMask(check.GetFocusObject().GetPhaseMask()), i_result(result), i_check(check) {}

        void Visit(CreatureMapType &m);
        {
            ..some code fast return if result found

            for(CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                if (!itr->getSource()->InSamePhase(i_phaseMask))
                    continue;

                if (!i_check(itr->getSource()))
                    continue;

                ..some code for update result and possible stop search
            }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED> &) {}
    };
    */

    // WorldObject searchers & workers

    template<class Check>
    struct WorldObjectSearcher
    {
        WorldObject*& i_object;
        Check& i_check;

        WorldObjectSearcher(WorldObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);
        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType& m);
        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct WorldObjectLastSearcher
    {
        WorldObject*& i_object;
        Check& i_check;

        WorldObjectLastSearcher(WorldObject* & result, Check& check) : i_object(result), i_check(check) {}

        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType& m);
        void Visit(GameObjectMapType& m);
        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct WorldObjectListSearcher
    {
        std::list<WorldObject*>& i_objects;
        Check& i_check;

        WorldObjectListSearcher(std::list<WorldObject*>& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType& m);
        void Visit(GameObjectMapType& m);
        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct WorldObjectWorker
    {
        Do const& i_do;

        explicit WorldObjectWorker(Do const& _do) : i_do(_do) {}

        void Visit(GameObjectMapType& m)
        {
            for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        void Visit(PlayerMapType& m)
        {
            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }
        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        void Visit(CorpseMapType& m)
        {
            for (CorpseMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        void Visit(DynamicObjectMapType& m)
        {
            for (DynamicObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Gameobject searchers

    template<class Check>
    struct GameObjectSearcher
    {
        GameObject*& i_object;
        Check& i_check;

        GameObjectSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check GO if any (Check can change requirements at each call)
    template<class Check>
    struct GameObjectLastSearcher
    {
        GameObject*& i_object;
        Check& i_check;

        GameObjectLastSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct GameObjectListSearcher
    {
        std::list<GameObject*>& i_objects;
        Check& i_check;

        GameObjectListSearcher(std::list<GameObject*>& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Unit searchers

    // First accepted by Check Unit if any
    template<class Check>
    struct UnitSearcher
    {
        Unit*& i_object;
        Check& i_check;

        UnitSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);
        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check Unit if any (Check can change requirements at each call)
    template<class Check>
    struct UnitLastSearcher
    {
        Unit*& i_object;
        Check& i_check;

        UnitLastSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);
        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // All accepted by Check units if any
    template<class Check>
    struct UnitListSearcher
    {
        std::list<Unit*>& i_objects;
        Check& i_check;

        UnitListSearcher(std::list<Unit*>& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // unit worker
    template<class Do>
    struct UnitWorker
    {
        Do& i_do;

        explicit UnitWorker(Do& _do) : i_do(_do) {}

        void Visit(PlayerMapType& m)
        {
            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }
        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Creature searchers

    template<class Check>
    struct CreatureSearcher
    {
        Creature*& i_object;
        Check& i_check;

        CreatureSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check Creature if any (Check can change requirements at each call)
    template<class Check>
    struct CreatureLastSearcher
    {
        Creature*& i_object;
        Check& i_check;

        CreatureLastSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct CreatureListSearcher
    {
        std::list<Creature*>& i_objects;
        Check& i_check;

        CreatureListSearcher(std::list<Creature*>& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct CreatureWorker
    {
        Do& i_do;

        CreatureWorker(WorldObject const* searcher, Do& _do) : i_do(_do) {}

        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Player searchers

    template<class Check>
    struct PlayerSearcher
    {
        Player*& i_object;
        Check& i_check;

        PlayerSearcher(Player*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct PlayerListSearcher
    {
        std::list<Player*>& i_objects;
        Check& i_check;

        PlayerListSearcher(std::list<Player*>& objects, Check& check)
            : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct PlayerWorker
    {
        Do& i_do;

        explicit PlayerWorker(Do& _do) : i_do(_do) {}

        void Visit(PlayerMapType& m)
        {
            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                { i_do(itr->getSource()); }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct CameraDistWorker
    {
        WorldObject const* i_searcher;
        float i_dist;
        Do& i_do;

        CameraDistWorker(WorldObject const* searcher, float _dist, Do& _do)
            : i_searcher(searcher), i_dist(_dist), i_do(_do) {}

        void Visit(CameraMapType& m)
        {
            for (CameraMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                Camera* camera = itr->getSource();
                if (camera->GetBody()->IsWithinDist(i_searcher, i_dist))
                    { i_do(camera->GetOwner()); }
            }
        }
        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // CHECKS && DO classes

    /* Model Check class:
    class SomeCheck
    {
        public:
            SomeCheck(SomeObjecType const* fobj, ..some other args) : i_fobj(fobj), ...other inits {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Creature* u)                    and for other intresting typs (Player/GameObject/Camera
            {
                return ..(code return true if Object fit to requirenment);
            }
            template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*) { return false; }
        private:
            SomeObjecType const* i_fobj;                    // Focus object used for check distance from, phase, so place in world
            ..other values need for check
    };
    */

    // WorldObject check classes
    class CannibalizeObjectCheck
    {
        public:
            CannibalizeObjectCheck(WorldObject const* fobj, float range) : i_fobj(fobj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Player* u)
            {
                if (i_fobj->IsFriendlyTo(u) || u->IsAlive() || u->IsTaxiFlying())
                    { return false; }

                return i_fobj->IsWithinDistInMap(u, i_range);
            }
            bool operator()(Corpse* u);
            bool operator()(Creature* u)
            {
                if (i_fobj->IsFriendlyTo(u) || u->IsAlive() || u->IsTaxiFlying() ||
                    (u->GetCreatureTypeMask() & CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) == 0)
                    { return false; }

                return i_fobj->IsWithinDistInMap(u, i_range);
            }
            template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*) { return false; }
        private:
            WorldObject const* i_fobj;
            float i_range;
    };

    // WorldObject do classes

    class RespawnDo
    {
        public:
            RespawnDo() {}
            void operator()(Creature* u) const;
            void operator()(GameObject* u) const;
            void operator()(WorldObject*) const {}
            void operator()(Corpse*) const {}
    };

    // GameObject checks

    class GameObjectFocusCheck
    {
        public:
            GameObjectFocusCheck(Unit const* unit, uint32 focusId) : i_unit(unit), i_focusId(focusId) {}
            WorldObject const& GetFocusObject() const { return *i_unit; }
            bool operator()(GameObject* go) const
            {
                GameObjectInfo const* goInfo = go->GetGOInfo();
                if (goInfo->type != GAMEOBJECT_TYPE_SPELL_FOCUS)
                    { return false; }

                if (goInfo->spellFocus.focusId != i_focusId)
                    { return false; }

                float dist = (float)goInfo->spellFocus.dist;

                return go->IsWithinDistInMap(i_unit, dist);
            }
        private:
            Unit const* i_unit;
            uint32 i_focusId;
    };

    // Find the nearest Fishing hole and return true only if source object is in range of hole
    class NearestGameObjectFishingHoleCheck
    {
        public:
            NearestGameObjectFishingHoleCheck(WorldObject const& obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(GameObject* go)
            {
                if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_FISHINGHOLE && go->isSpawned() && i_obj.IsWithinDistInMap(go, i_range) && i_obj.IsWithinDistInMap(go, (float)go->GetGOInfo()->fishinghole.radius))
                {
                    i_range = i_obj.GetDistance(go);
                    return true;
                }
                return false;
            }
            float GetLastRange() const { return i_range; }
        private:
            WorldObject const& i_obj;
            float  i_range;

            // prevent clone
            NearestGameObjectFishingHoleCheck(NearestGameObjectFishingHoleCheck const&);
    };

    // Success at unit in range, range update for next check (this can be use with GameobjectLastSearcher to find nearest GO)
    class NearestGameObjectEntryInObjectRangeCheck
    {
        public:
            NearestGameObjectEntryInObjectRangeCheck(WorldObject const& obj, uint32 entry, float range) : i_obj(obj), i_entry(entry), i_range(range) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(GameObject* go)
            {
                if (go->GetEntry() == i_entry && i_obj.IsWithinDistInMap(go, i_range))
                {
                    i_range = i_obj.GetDistance(go);        // use found GO range as new range limit for next check
                    return true;
                }
                return false;
            }
            float GetLastRange() const { return i_range; }
        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float  i_range;

            // prevent clone this object
            NearestGameObjectEntryInObjectRangeCheck(NearestGameObjectEntryInObjectRangeCheck const&);
    };

    // Success at gameobject in range of xyz, range update for next check (this can be use with GameobjectLastSearcher to find nearest GO)
    class NearestGameObjectEntryInPosRangeCheck
    {
        public:
            NearestGameObjectEntryInPosRangeCheck(WorldObject const& obj, uint32 entry, float x, float y, float z, float range)
                : i_obj(obj), i_entry(entry), i_x(x), i_y(y), i_z(z), i_range(range) {}

            WorldObject const& GetFocusObject() const { return i_obj; }

            bool operator()(GameObject* go)
            {
                if (go->GetEntry() == i_entry && go->IsWithinDist3d(i_x, i_y, i_z, i_range))
                {
                    // use found GO range as new range limit for next check
                    i_range = go->GetDistance(i_x, i_y, i_z);
                    return true;
                }

                return false;
            }

            float GetLastRange() const { return i_range; }

        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float i_x, i_y, i_z;
            float i_range;

            // prevent clone this object
            NearestGameObjectEntryInPosRangeCheck(NearestGameObjectEntryInPosRangeCheck const&);
    };

    // Success at gameobject with entry in range of provided xyz
    class GameObjectEntryInPosRangeCheck
    {
        public:
            GameObjectEntryInPosRangeCheck(WorldObject const& obj, uint32 entry, float x, float y, float z, float range)
                : i_obj(obj), i_entry(entry), i_x(x), i_y(y), i_z(z), i_range(range) {}

            WorldObject const& GetFocusObject() const { return i_obj; }

            bool operator()(GameObject* go)
            {
                if (go->GetEntry() == i_entry && go->IsWithinDist3d(i_x, i_y, i_z, i_range))
                    { return true; }

                return false;
            }

            float GetLastRange() const { return i_range; }

        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float i_x, i_y, i_z;
            float i_range;

            // prevent clone this object
            GameObjectEntryInPosRangeCheck(GameObjectEntryInPosRangeCheck const&);
    };

    // Unit checks

    class MostHPMissingInRangeCheck
    {
        public:
            MostHPMissingInRangeCheck(Unit const* obj, float range, uint32 hp) : i_obj(obj), i_range(range), i_hp(hp) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && u->GetMaxHealth() - u->GetHealth() > i_hp)
                {
                    i_hp = u->GetMaxHealth() - u->GetHealth();
                    return true;
                }
                return false;
            }
        private:
            Unit const* i_obj;
            float i_range;
            uint32 i_hp;
    };

    class FriendlyCCedInRangeCheck
    {
        public:
            FriendlyCCedInRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) &&
                    (u->IsCharmed() || u->IsFrozen() || u->hasUnitState(UNIT_STAT_CAN_NOT_REACT)))
                {
                    return true;
                }
                return false;
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    class FriendlyMissingBuffInRangeCheck
    {
        public:
            FriendlyMissingBuffInRangeCheck(WorldObject const* obj, float range, uint32 spellid) : i_obj(obj), i_range(range), i_spell(spellid) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) &&
                    !(u->HasAura(i_spell, EFFECT_INDEX_0) || u->HasAura(i_spell, EFFECT_INDEX_1) || u->HasAura(i_spell, EFFECT_INDEX_2)))
                {
                    return true;
                }
                return false;
            }
        private:
            WorldObject const* i_obj;
            float i_range;
            uint32 i_spell;
    };

    class AnyUnfriendlyUnitInObjectRangeCheck
    {
        public:
            AnyUnfriendlyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range)
            {
                i_controlledByPlayer = obj->IsControlledByPlayer();
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && (i_controlledByPlayer ? !i_obj->IsFriendlyTo(u) : i_obj->IsHostileTo(u))
                    && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }
                else
                    { return false; }
            }
        private:
            WorldObject const* i_obj;
            bool i_controlledByPlayer;
            float i_range;
    };

    class AnyUnfriendlyVisibleUnitInObjectRangeCheck
    {
        public:
            AnyUnfriendlyVisibleUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range)
                : i_obj(obj), i_funit(funit), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive()
                       && i_obj->IsWithinDistInMap(u, i_range)
                       && !i_funit->IsFriendlyTo(u)
                       && u->IsVisibleForOrDetect(i_funit, i_funit, false);
            }
        private:
            WorldObject const* i_obj;
            Unit const* i_funit;
            float i_range;
    };

    class AnyFriendlyUnitInObjectRangeCheck
    {
        public:
            AnyFriendlyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range) && i_obj->IsFriendlyTo(u))
                    { return true; }
                else
                    { return false; }
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    class AnyUnitInObjectRangeCheck
    {
        public:
            AnyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }

                return false;
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    // Success at unit in range, range update for next check (this can be use with UnitLastSearcher to find nearest unit)
    class NearestAttackableUnitInObjectRangeCheck
    {
        public:
            NearestAttackableUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range) : i_obj(obj), i_funit(funit), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (u->IsTargetableForAttack() && i_obj->IsWithinDistInMap(u, i_range) &&
                    !i_funit->IsFriendlyTo(u) && u->IsVisibleForOrDetect(i_funit, i_funit, false))
                {
                    i_range = i_obj->GetDistance(u);        // use found unit range as new range limit for next check
                    return true;
                }

                return false;
            }
        private:
            WorldObject const* i_obj;
            Unit const* i_funit;
            float i_range;

            // prevent clone this object
            NearestAttackableUnitInObjectRangeCheck(NearestAttackableUnitInObjectRangeCheck const&);
    };

    class AnyAoEVisibleTargetUnitInObjectRangeCheck
    {
        public:
            AnyAoEVisibleTargetUnitInObjectRangeCheck(WorldObject const* obj, WorldObject const* originalCaster, float range)
                : i_obj(obj), i_originalCaster(originalCaster), i_range(range)
            {
                i_targetForUnit = i_originalCaster->isType(TYPEMASK_UNIT);
                i_targetForPlayer = (i_originalCaster->GetTypeId() == TYPEID_PLAYER);
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
                if (!u->IsTargetableForAttack())
                    { return false; }

                // ignore totems as AoE targets
                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    { return false; }

                // check visibility only for unit-like original casters
                if (i_targetForUnit && !u->IsVisibleForOrDetect((Unit const*)i_originalCaster, i_originalCaster, false))
                    { return false; }

                if ((i_targetForPlayer ? !i_originalCaster->IsFriendlyTo(u) : i_originalCaster->IsHostileTo(u)) && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }

                return false;
            }
        private:
            WorldObject const* i_obj;
            WorldObject const* i_originalCaster;
            float i_range;
            bool i_targetForUnit;
            bool i_targetForPlayer;
    };

    class AnyAoETargetUnitInObjectRangeCheck
    {
        public:
            AnyAoETargetUnitInObjectRangeCheck(WorldObject const* obj, float range)
                : i_obj(obj), i_range(range)
            {
                i_targetForPlayer = i_obj->IsControlledByPlayer();
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
                if (!u->IsTargetableForAttack())
                    { return false; }

                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    { return false; }

                if ((i_targetForPlayer ? !i_obj->IsFriendlyTo(u) : i_obj->IsHostileTo(u)) && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }

                return false;
            }

        private:
            WorldObject const* i_obj;
            float i_range;
            bool i_targetForPlayer;
    };

    class AnySpecificUnitInGameObjectRangeCheck
    {
        public:
            AnySpecificUnitInGameObjectRangeCheck(GameObject* go, float range, bool friendly = true)
                : i_obj(go), i_range(range), i_isFriendly(friendly)
            {
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
                if (!u->IsTargetableForAttack())
                    { return false; }

                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    { return false; }

                if ((i_isFriendly ? i_obj->IsFriendlyTo(u) : i_obj->IsHostileTo(u)) && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }

                return false;
            }
        private:
            GameObject*         i_obj;
            float               i_range;
            bool                i_isFriendly;
    };

    class AllSpecificUnitsInGameObjectRangeDo
    {
        public:
            AllSpecificUnitsInGameObjectRangeDo(GameObject* go, float range, bool friendly = true)
                : i_obj(go), i_range(range), i_isFriendly(friendly)
            {
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            void operator()(Unit* u)
            {
                // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
                if (!u->IsTargetableForAttack())
                    { return; }

                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    { return; }

                if ((i_isFriendly ? i_obj->IsFriendlyTo(u) : i_obj->IsHostileTo(u)) && i_obj->IsWithinDistInMap(u, i_range))
                    { i_obj->Use(u); }
            }
        private:
            GameObject*         i_obj;
            float               i_range;
            bool                i_isFriendly;
    };

    // do attack at call of help to friendly crearture
    class CallOfHelpCreatureInRangeDo
    {
        public:
            CallOfHelpCreatureInRangeDo(Unit* funit, Unit* enemy, float range)
                : i_funit(funit), i_enemy(enemy), i_range(range)
            {}
            void operator()(Creature* u);

        private:
            Unit* const i_funit;
            Unit* const i_enemy;
            float i_range;
    };

    class AnyDeadUnitCheck
    {
        public:
            explicit AnyDeadUnitCheck(WorldObject const* fobj) : i_fobj(fobj) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Unit* u) { return !u->IsAlive(); }
        private:
            WorldObject const* i_fobj;
    };

    class AnyStealthedCheck
    {
        public:
            explicit AnyStealthedCheck(WorldObject const* fobj) : i_fobj(fobj) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Unit* u) { return u->GetVisibility() == VISIBILITY_GROUP_STEALTH; }
        private:
            WorldObject const* i_fobj;
    };

    // Creature checks

    class InAttackDistanceFromAnyHostileCreatureCheck
    {
        public:
            explicit InAttackDistanceFromAnyHostileCreatureCheck(Unit* funit) : i_funit(funit) {}
            WorldObject const& GetFocusObject() const { return *i_funit; }
            bool operator()(Creature* u)
            {
                if (u->IsAlive() && u->IsHostileTo(i_funit) && i_funit->IsWithinDistInMap(u, u->GetAttackDistance(i_funit)))
                    { return true; }

                return false;
            }
        private:
            Unit* const i_funit;
    };

    class AnyAssistCreatureInRangeCheck
    {
        public:
            AnyAssistCreatureInRangeCheck(Unit* funit, Unit* enemy, float range)
                : i_funit(funit), i_enemy(enemy), i_range(range)
            {
            }
            WorldObject const& GetFocusObject() const { return *i_funit; }
            bool operator()(Creature* u);

        private:
            Unit* const i_funit;
            Unit* const i_enemy;
            float i_range;
    };

    class NearestAssistCreatureInCreatureRangeCheck
    {
        public:
            NearestAssistCreatureInCreatureRangeCheck(Creature* obj, Unit* enemy, float range)
                : i_obj(obj), i_enemy(enemy), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Creature* u)
            {
                if (u == i_obj)
                    { return false; }
                if (!u->CanAssistTo(i_obj, i_enemy))
                    { return false; }

                if (!i_obj->IsWithinDistInMap(u, i_range))
                    { return false; }

                if (!i_obj->IsWithinLOSInMap(u))
                    { return false; }

                i_range = i_obj->GetDistance(u);            // use found unit range as new range limit for next check
                return true;
            }
            float GetLastRange() const { return i_range; }
        private:
            Creature* const i_obj;
            Unit* const i_enemy;
            float  i_range;

            // prevent clone this object
            NearestAssistCreatureInCreatureRangeCheck(NearestAssistCreatureInCreatureRangeCheck const&);
    };

    // Success at unit in range, range update for next check (this can be use with CreatureLastSearcher to find nearest creature)
    class NearestCreatureEntryWithLiveStateInObjectRangeCheck
    {
        public:
            NearestCreatureEntryWithLiveStateInObjectRangeCheck(WorldObject const& obj, uint32 entry, bool onlyAlive, bool onlyDead, float range, bool excludeSelf = false)
                : i_obj(obj), i_entry(entry), i_onlyAlive(onlyAlive), i_onlyDead(onlyDead), i_excludeSelf(excludeSelf), i_range(range) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(Creature* u)
            {
                if (u->GetEntry() == i_entry && ((i_onlyAlive && u->IsAlive()) || (i_onlyDead && u->IsCorpse()) || (!i_onlyAlive && !i_onlyDead))
                    && (!i_excludeSelf || &i_obj != u) && i_obj.IsWithinDistInMap(u, i_range))
                {
                    i_range = i_obj.GetDistance(u);         // use found unit range as new range limit for next check
                    return true;
                }
                return false;
            }
            float GetLastRange() const { return i_range; }
        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            bool   i_onlyAlive;
            bool   i_onlyDead;
            bool   i_excludeSelf;
            float  i_range;

            // prevent clone this object
            NearestCreatureEntryWithLiveStateInObjectRangeCheck(NearestCreatureEntryWithLiveStateInObjectRangeCheck const&);
    };

    class AllCreaturesOfEntryInRangeCheck
    {
        public:
            AllCreaturesOfEntryInRangeCheck(const WorldObject* pObject, uint32 uiEntry, float fMaxRange) : m_pObject(pObject), m_uiEntry(uiEntry), m_fRange(fMaxRange) {}
            WorldObject const& GetFocusObject() const { return *m_pObject; }
            bool operator()(Unit* pUnit)
            {
                if (pUnit->GetEntry() == m_uiEntry && m_pObject->IsWithinDist(pUnit, m_fRange, false))
                    { return true; }

                return false;
            }

        private:
            const WorldObject* m_pObject;
            uint32 m_uiEntry;
            float m_fRange;

            // prevent clone this object
            AllCreaturesOfEntryInRangeCheck(AllCreaturesOfEntryInRangeCheck const&);
    };

    // Player checks and do

    class AnyPlayerInObjectRangeCheck
    {
        public:
            AnyPlayerInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range))
                    { return true; }

                return false;
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    class AnyPlayerInObjectRangeWithAuraCheck
    {
        public:
            AnyPlayerInObjectRangeWithAuraCheck(WorldObject const* obj, float range, uint32 spellId)
                : i_obj(obj), i_range(range), i_spellId(spellId) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                return u->IsAlive()
                       && i_obj->IsWithinDistInMap(u, i_range)
                       && u->HasAura(i_spellId);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
            uint32 i_spellId;
    };

    class AnyPlayerInCapturePointRange
    {
        public:
            AnyPlayerInCapturePointRange(WorldObject const* obj, float range)
                : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                return u->CanUseCapturePoint() &&
                       i_obj->IsWithinDistInMap(u, i_range);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    // Prepare using Builder localized packets with caching and send to player
    template<class Builder>
    class LocalizedPacketDo
    {
        public:
            explicit LocalizedPacketDo(Builder& builder) : i_builder(builder) {}

            ~LocalizedPacketDo()
            {
                for (size_t i = 0; i < i_data_cache.size(); ++i)
                    { delete i_data_cache[i]; }
            }
            void operator()(Player* p);

        private:
            Builder& i_builder;
            std::vector<WorldPacket*> i_data_cache;         // 0 = default, i => i-1 locale index
    };

    // Prepare using Builder localized packets with caching and send to player
    template<class Builder>
    class LocalizedPacketListDo
    {
        public:
            typedef std::vector<WorldPacket*> WorldPacketList;
            explicit LocalizedPacketListDo(Builder& builder) : i_builder(builder) {}

            ~LocalizedPacketListDo()
            {
                for (size_t i = 0; i < i_data_cache.size(); ++i)
                    for (size_t j = 0; j < i_data_cache[i].size(); ++j)
                        { delete i_data_cache[i][j]; }
            }
            void operator()(Player* p);

        private:
            Builder& i_builder;
            std::vector<WorldPacketList> i_data_cache;
            // 0 = default, i => i-1 locale index
    };

#ifndef WIN32
    template<> void PlayerRelocationNotifier::Visit<Creature>(CreatureMapType&);
    template<> void CreatureRelocationNotifier::Visit<Player>(PlayerMapType&);
    template<> void CreatureRelocationNotifier::Visit<Creature>(CreatureMapType&);
    template<> inline void DynamicObjectUpdater::Visit<Creature>(CreatureMapType&);
    template<> inline void DynamicObjectUpdater::Visit<Player>(PlayerMapType&);
#endif
}
#endif
