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

#include "Camera.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include "Errors.h"
#include "Player.h"

/**
 * @brief Creates a camera bound to a player.
 *
 * @param pl The player that owns this camera.
 */
Camera::Camera(Player* pl) : m_owner(*pl), m_source(pl)
{
    m_source->GetViewPoint().Attach(this);
}

/**
 * @brief Destroys the camera and detaches it from the current viewpoint.
 */
Camera::~Camera()
{
    // view of camera should be already reseted to owner (RemoveFromWorld -> Event_RemovedFromWorld -> ResetView)
    MANGOS_ASSERT(m_source == &m_owner);

    // for symmetry with constructor and way to make viewpoint's list empty
    m_source->GetViewPoint().Detach(this);
}

/**
 * @brief Forwards a packet to the camera owner.
 *
 * @param data The packet to send.
 */
void Camera::ReceivePacket(WorldPacket* data)
{
    m_owner.SendDirectMessage(data);
}

/**
 * @brief Updates camera registration for the current viewpoint.
 *
 * Rebinds the camera to the source grid and refreshes owner visibility state.
 */
void Camera::UpdateForCurrentViewPoint()
{
    m_gridRef.unlink();

    if (GridType* grid = m_source->GetViewPoint().m_grid)
    {
        grid->AddWorldObject(this);
    }

    UpdateVisibilityForOwner();
}

/**
 * @brief Changes the camera viewpoint.
 *
 * Moves the camera from its current source to a new world object and optionally
 * updates the player's farsight field.
 *
 * @param obj The new viewpoint object.
 * @param update_far_sight_field true to update the player's farsight field; otherwise, false.
 */
void Camera::SetView(WorldObject* obj, bool update_far_sight_field /*= true*/)
{
    MANGOS_ASSERT(obj);

    if (m_source == obj)
    {
        return;
    }

    if (!m_owner.IsInMap(obj))
    {
        sLog.outError("Camera::SetView, viewpoint is not in map with camera's owner");
        return;
    }

    if (!obj->isType(TypeMask(TYPEMASK_DYNAMICOBJECT | TYPEMASK_UNIT)))
    {
        sLog.outError("Camera::SetView, viewpoint type is not available for client");
        return;
    }

    // detach and deregister from active objects if there are no more reasons to be active
    m_source->GetViewPoint().Detach(this);
    if (!m_source->IsActiveObject())
    {
        m_source->GetMap()->RemoveFromActive(m_source);
    }

    m_source = obj;

    if (!m_source->IsActiveObject())
    {
        m_source->GetMap()->AddToActive(m_source);
    }

    m_source->GetViewPoint().Attach(this);

    if (update_far_sight_field)
    {
        m_owner.SetGuidValue(PLAYER_FARSIGHT, (m_source == &m_owner ? ObjectGuid() : m_source->GetObjectGuid()));
    }

    UpdateForCurrentViewPoint();
}

/**
 * @brief Handles visibility loss for the active viewpoint.
 *
 * Resets the camera when the owner can no longer see the current source object.
 */
void Camera::Event_ViewPointVisibilityChanged()
{
    if (!m_owner.HaveAtClient(m_source))
    {
        ResetView();
    }
}

/**
 * @brief Restores the camera view to its owner.
 *
 * @param update_far_sight_field true to update the player's farsight field; otherwise, false.
 */
void Camera::ResetView(bool update_far_sight_field /*= true*/)
{
    SetView(&m_owner, update_far_sight_field);
}

/**
 * @brief Handles the camera being added to the world.
 */
void Camera::Event_AddedToWorld()
{
    GridType* grid = m_source->GetViewPoint().m_grid;
    MANGOS_ASSERT(grid);
    grid->AddWorldObject(this);

    UpdateVisibilityForOwner();
}

/**
 * @brief Handles the camera being removed from the world.
 */
void Camera::Event_RemovedFromWorld()
{
    if (m_source == &m_owner)
    {
        m_gridRef.unlink();
        return;
    }

    ResetView();
}

/**
 * @brief Handles viewpoint movement updates.
 */
void Camera::Event_Moved()
{
    m_gridRef.unlink();
    m_source->GetViewPoint().m_grid->AddWorldObject(this);
}

/**
 * @brief Updates owner visibility for a specific target.
 *
 * @param target The world object whose visibility should be updated.
 */
void Camera::UpdateVisibilityOf(WorldObject* target)
{
    m_owner.UpdateVisibilityOf(m_source, target);
}

/**
 * @brief Updates owner visibility for a target using accumulated update data.
 *
 * @param target The world object whose visibility should be updated.
 * @param data The update packet data being built.
 * @param vis The set of currently visible objects.
 */
void Camera::UpdateVisibilityOf(WorldObject* target, UpdateData& data, std::set<WorldObject*>& vis)
{
    m_owner.UpdateVisibilityOf(m_source, target, data, vis);
}

/**
 * @brief Rebuilds visibility for the camera owner around the current source.
 */
void Camera::UpdateVisibilityForOwner()
{
    MaNGOS::VisibleNotifier notifier(*this);
    Cell::VisitAllObjects(m_source, notifier, m_source->GetMap()->GetVisibilityDistance(), false);
    notifier.Notify();
}

//////////////////

/**
 * @brief Destroys a viewpoint instance.
 */
ViewPoint::~ViewPoint()
{
    if (!m_cameras.empty())
    {
        sLog.outError("ViewPoint destructor called, but some cameras referenced to it");
    }
}
