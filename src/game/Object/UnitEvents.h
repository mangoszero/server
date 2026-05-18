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

#ifndef _UNITEVENTS
#define _UNITEVENTS

#include "Common.h"

class ThreatContainer;
class ThreatManager;
class HostileReference;

/**
 * @brief Enum for different types of unit threat events.
 */
enum UnitThreatEventType
{
    // Player/Pet changed on/offline status
    UEV_THREAT_REF_ONLINE_STATUS        = 1 << 0,

    // Threat for Player/Pet changed
    UEV_THREAT_REF_THREAT_CHANGE        = 1 << 1,

    // Player/Pet will be removed from list (dead) [for internal use]
    UEV_THREAT_REF_REMOVE_FROM_LIST     = 1 << 2,

    // Player/Pet entered/left water or some other place where it is/was not accessible for the creature
    UEV_THREAT_REF_ASSECCIBLE_STATUS    = 1 << 3,

    // Threat list is going to be sorted (if dirty flag is set)
    UEV_THREAT_SORT_LIST                = 1 << 4,

    // New target should be fetched, could be the current target as well
    UEV_THREAT_SET_NEXT_TARGET          = 1 << 5,

    // A new victim (target) was set. Could be NULL
    UEV_THREAT_VICTIM_CHANGED           = 1 << 6,
};

#define UEV_THREAT_REF_EVENT_MASK ( UEV_THREAT_REF_ONLINE_STATUS | UEV_THREAT_REF_THREAT_CHANGE | UEV_THREAT_REF_REMOVE_FROM_LIST | UEV_THREAT_REF_ASSECCIBLE_STATUS)
#define UEV_THREAT_MANAGER_EVENT_MASK (UEV_THREAT_SORT_LIST | UEV_THREAT_SET_NEXT_TARGET | UEV_THREAT_VICTIM_CHANGED)
#define UEV_ALL_EVENT_MASK (0xffffffff)

// Future use
//#define UEV_UNIT_EVENT_MASK (UEV_UNIT_KILLED | UEV_UNIT_HEALTH_CHANGE)

/**
 * @brief Base class for unit events.
 */
class UnitBaseEvent
{
    private:
        uint32 iType; /**< The type of the event. */
    public:
        /**
         * @brief Constructor for UnitBaseEvent.
         *
         * @param pType The type of the event.
         */
        UnitBaseEvent(uint32 pType) { iType = pType; }

        /**
         * @brief Gets the type of the event.
         *
         * @return uint32 The type of the event.
         */
        uint32 getType() const { return iType; }

        /**
         * @brief Checks if the event matches a type mask.
         *
         * @param pMask The type mask.
         * @return bool True if the event matches the type mask.
         */
        bool matchesTypeMask(uint32 pMask) const { return iType & pMask; }

        /**
         * @brief Sets the type of the event.
         *
         * @param pType The type of the event.
         */
        void setType(uint32 pType) { iType = pType; }
};

/**
 * @brief Class for threat reference status change events.
 */
class ThreatRefStatusChangeEvent : public UnitBaseEvent
{
    private:
        HostileReference* iHostileReference; /**< The hostile reference associated with the event. */
        union
        {
            float iFValue; /**< Float value associated with the event. */
            int32 iIValue; /**< Integer value associated with the event. */
            bool iBValue; /**< Boolean value associated with the event. */
        };
        ThreatManager* iThreatManager; /**< The threat manager associated with the event. */
    public:
        /**
         * @brief Constructor for ThreatRefStatusChangeEvent.
         *
         * @param pType The type of the event.
         */
        ThreatRefStatusChangeEvent(uint32 pType) : UnitBaseEvent(pType)
        {
            iHostileReference = nullptr;
            iFValue = 0.0f;
            iIValue = 0;
            iBValue = false;
            iThreatManager = nullptr;
        }

        /**
         * @brief Constructor for ThreatRefStatusChangeEvent with a hostile reference.
         *
         * @param pType The type of the event.
         * @param pHostileReference The hostile reference associated with the event.
         */
        ThreatRefStatusChangeEvent(uint32 pType, HostileReference* pHostileReference) : UnitBaseEvent(pType)
        {
            iHostileReference = pHostileReference;
            iFValue = 0.0f;
            iIValue = 0;
            iBValue = false;
            iThreatManager = nullptr;
        }

        /**
         * @brief Constructor for ThreatRefStatusChangeEvent with a float value.
         *
         * @param pType The type of the event.
         * @param pHostileReference The hostile reference associated with the event.
         * @param pValue The float value associated with the event.
         */
        ThreatRefStatusChangeEvent(uint32 pType, HostileReference* pHostileReference, float pValue) : UnitBaseEvent(pType)
        {
            iHostileReference = pHostileReference;
            // ifValue in union, so don't alter other values
            iFValue = pValue;
            iThreatManager = nullptr;
        }

        /**
         * @brief Constructor for ThreatRefStatusChangeEvent with a boolean value.
         *
         * @param pType The type of the event.
         * @param pHostileReference The hostile reference associated with the event.
         * @param pValue The boolean value associated with the event.
         */
        ThreatRefStatusChangeEvent(uint32 pType, HostileReference* pHostileReference, bool pValue) : UnitBaseEvent(pType)
        {
            iHostileReference = pHostileReference;
            // iBValue in union, so don't alter other values
            iBValue = pValue;
            iThreatManager = nullptr;
        }

        /**
         * @brief Gets the integer value associated with the event.
         *
         * @return int32 The integer value.
         */
        int32 getIValue() const { return iIValue; }

        /**
         * @brief Gets the float value associated with the event.
         *
         * @return float The float value.
         */
        float getFValue() const { return iFValue; }

        /**
         * @brief Gets the boolean value associated with the event.
         *
         * @return bool The boolean value.
         */
        bool getBValue() const { return iBValue; }

        /**
         * @brief Sets the boolean value associated with the event.
         *
         * @param pValue The boolean value.
         */
        void setBValue(bool pValue) { iBValue = pValue; }

        /**
         * @brief Gets the hostile reference associated with the event.
         *
         * @return HostileReference* The hostile reference.
         */
        HostileReference* getReference() const { return iHostileReference; }

        /**
         * @brief Sets the threat manager associated with the event.
         *
         * @param pThreatManager The threat manager.
         */
        void setThreatManager(ThreatManager* pThreatManager) { iThreatManager = pThreatManager; }

        /**
         * @brief Gets the threat manager associated with the event.
         *
         * @return ThreatManager* The threat manager.
         */
        ThreatManager* GetThreatManager() const { return iThreatManager; }
};

/**
 * @brief Class for threat manager events.
 */
class ThreatManagerEvent : public ThreatRefStatusChangeEvent
{
    private:
        ThreatContainer* iThreatContainer; /**< The threat container associated with the event. */
    public:
        /**
         * @brief Constructor for ThreatManagerEvent.
         *
         * @param pType The type of the event.
         */
        ThreatManagerEvent(uint32 pType) : ThreatRefStatusChangeEvent(pType)
        {
            iThreatContainer = nullptr;
        }

        /**
         * @brief Constructor for ThreatManagerEvent with a hostile reference.
         *
         * @param pType The type of the event.
         * @param pHostileReference The hostile reference associated with the event.
         */
        ThreatManagerEvent(uint32 pType, HostileReference* pHostileReference) : ThreatRefStatusChangeEvent(pType, pHostileReference)
        {
            iThreatContainer = nullptr;
        }


        /**
         * @brief Sets the threat container associated with the event.
         *
         * @param pThreatContainer The threat container.
         */
        void setThreatContainer(ThreatContainer* pThreatContainer) { iThreatContainer = pThreatContainer; }

        /**
         * @brief Gets the threat container associated with the event.
         *
         * @return ThreatContainer* The threat container.
         */
        ThreatContainer* getThreatContainer() const { return iThreatContainer; }
};

#endif
