#pragma once

class PlayerbotAI;

namespace ai
{
    class AiObjectContext;
    class ChatHelper;

    class AiObject : public PlayerbotAIAware
    {
        public:
            AiObject(PlayerbotAI* ai);

            // Every context deletes what it created through a base pointer --
            // NamedObjectContext<T>::Clear does `delete *i` over vector<T*> -- and T is
            // Action, Trigger, Strategy or UntypedValue. The first three each declare a
            // virtual destructor of their own; UntypedValue does not, so deleting a value
            // through it ran only ~UntypedValue and left every derived member alive. The
            // list nodes in the ObjectGuidList values, the Item* list in
            // InventoryItemValue and the string buffer in RtiValue were all leaked, on
            // every bot logout and every random-bot removal. Declaring it here fixes the
            // whole hierarchy at once rather than patching each root separately.
            virtual ~AiObject() {}

        protected:
            Player* bot;
            Player* GetMaster();
            AiObjectContext* context;
            ChatHelper* chat;
    };

    class AiNamedObject : public AiObject
    {
        public:
            AiNamedObject(PlayerbotAI* ai, string name) : AiObject(ai), name(name) {}

        public:
            virtual string getName()
            {
                return name;
            }

        protected:
            string name;
    };
}
