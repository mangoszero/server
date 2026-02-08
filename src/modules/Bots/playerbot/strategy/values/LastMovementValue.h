#pragma once
#include "../Value.h"

namespace ai
{
    // Class to store the last movement details of a unit
    class LastMovement
    {
    public:
        // Default constructor initializing all member variables
        LastMovement()
        {
            lastMoveToX = 0;
            lastMoveToY = 0;
            lastMoveToZ = 0;
            lastMoveToOri = 0;
            lastFollow = NULL;
            lastAreaTrigger = 0; // Initialize lastAreaTrigger
            lastFollowState = false;
        }

        // Copy constructor to copy movement details from another LastMovement object
        LastMovement(LastMovement& other)
        {
            taxiNodes = other.taxiNodes;
            taxiMaster = other.taxiMaster;
            lastFollow = other.lastFollow;
            lastAreaTrigger = other.lastAreaTrigger;
            lastMoveToX = other.lastMoveToX;
            lastMoveToY = other.lastMoveToY;
            lastMoveToZ = other.lastMoveToZ;
            lastMoveToOri = other.lastMoveToOri;
            lastFollowState = other.lastFollowState;
        }

        // Set the last follow unit and reset movement coordinates
        void Set(Unit* lastFollow)
        {
            Set(0.0f, 0.0f, 0.0f, 0.0f);
            this->lastFollow = lastFollow;
        }

        // Set the last movement coordinates and orientation
        void Set(float x, float y, float z, float ori)
        {
            lastMoveToX = x;
            lastMoveToY = y;
            lastMoveToZ = z;
            lastMoveToOri = ori;
            lastFollow = NULL;
        }

    public:
        vector<uint32> taxiNodes; // List of taxi nodes
        ObjectGuid taxiMaster; // GUID of the taxi master
        Unit* lastFollow; // Pointer to the last followed unit
        uint32 lastAreaTrigger; // ID of the last area trigger
        bool lastFollowState; // whether follow was removed temprarily
        float lastMoveToX, lastMoveToY, lastMoveToZ, lastMoveToOri; // Last movement coordinates and orientation
    };

    // Class to manage the last movement value
    class LastMovementValue : public ManualSetValue<LastMovement&>
    {
    public:
        // Constructor initializing the LastMovementValue with a PlayerbotAI instance
        LastMovementValue(PlayerbotAI* ai) : ManualSetValue<LastMovement&>(ai, data) {}

    private:
        LastMovement data; // Instance of LastMovement to store movement data
    };
}
