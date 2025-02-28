#pragma once

using namespace std;

namespace ai
{
    enum LootStrategy
    {
        LOOTSTRATEGY_QUEST = 1,
        LOOTSTRATEGY_SKILL = 2,
        LOOTSTRATEGY_GRAY = 3,
        LOOTSTRATEGY_NORMAL = 4,
        LOOTSTRATEGY_ALL = 5
    };

    /**
     * @brief Represents a lootable object.
     */
    class LootObject
    {
    public:
        LootObject() {}
        LootObject(Player* bot, ObjectGuid guid);
        LootObject(const LootObject& other);

    public:
        /**
         * @brief Checks if the loot object is empty.
         *
         * @return true if the loot object is empty, false otherwise.
         */
        bool IsEmpty() { return !guid; }

        /**
         * @brief Checks if looting is possible.
         *
         * @param bot The player bot.
         * @return true if looting is possible, false otherwise.
         */
        bool IsLootPossible(Player* bot);

        /**
         * @brief Refreshes the loot object with a new GUID.
         *
         * @param bot The player bot.
         * @param guid The new GUID.
         */
        void Refresh(Player* bot, ObjectGuid guid);

        /**
         * @brief Gets the world object associated with the loot object.
         *
         * @param bot The player bot.
         * @return The world object.
         */
        WorldObject* GetWorldObject(Player* bot);

    public:
        ObjectGuid guid; ///< The GUID of the loot object.
        uint32 skillId; ///< The skill ID required to loot the object.
        uint32 reqSkillValue; ///< The required skill value to loot the object.
        uint32 reqItem; ///< The required item to loot the object.
    };

    /**
     * @brief Represents a loot target.
     */
    class LootTarget
    {
    public:
        LootTarget(ObjectGuid guid);
        LootTarget(LootTarget const& other);

    public:
        LootTarget& operator=(LootTarget const& other);
        bool operator< (const LootTarget& other) const;

    public:
        ObjectGuid guid; ///< The GUID of the loot target.
        time_t asOfTime; ///< The time when the loot target was added.
    };

    /**
     * @brief Represents a list of loot targets.
     */
    class LootTargetList : public set<LootTarget>
    {
    public:
        /**
         * @brief Shrinks the list by removing targets older than the specified time.
         *
         * @param fromTime The time threshold.
         */
        void shrink(time_t fromTime);
    };

    /**
     * @brief Represents a stack of loot objects.
     */
    class LootObjectStack
    {
    public:
        LootObjectStack(Player* bot) : bot(bot) {}

    public:
        /**
         * @brief Adds a loot object to the stack.
         *
         * @param guid The GUID of the loot object.
         * @return true if the loot object was added, false otherwise.
         */
        bool Add(ObjectGuid guid);

        /**
         * @brief Removes a loot object from the stack.
         *
         * @param guid The GUID of the loot object.
         */
        void Remove(ObjectGuid guid);

        /**
         * @brief Clears the stack of loot objects.
         */
        void Clear();

        /**
         * @brief Checks if looting is possible within the specified distance.
         *
         * @param maxDistance The maximum distance to check.
         * @return true if looting is possible, false otherwise.
         */
        bool CanLoot(float maxDistance);

        /**
         * @brief Gets the loot object within the specified distance.
         *
         * @param maxDistance The maximum distance to check.
         * @return The loot object.
         */
        LootObject GetLoot(float maxDistance = 0);

    private:
        /**
         * @brief Orders the loot objects by distance.
         *
         * @param maxDistance The maximum distance to check.
         * @return A vector of ordered loot objects.
         */
        vector<LootObject> OrderByDistance(float maxDistance = 0);

    private:
        Player* bot; ///< The player bot.
        LootTargetList availableLoot; ///< The list of available loot targets.
    };

};
