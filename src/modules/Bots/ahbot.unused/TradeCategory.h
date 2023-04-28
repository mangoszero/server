#pragma once
#include "Config/Config.h"
#include "Category.h"

using namespace std;

namespace ahbot
{
    class Engineering : public Trade
    {
    public:
        Engineering() : Trade() {}

    public:
        virtual bool Contains(ItemPrototype const* proto)
        {
            return Trade::Contains(proto) &&
                    (proto->SubClass == ITEM_SUBCLASS_PARTS ||
                    proto->SubClass == ITEM_SUBCLASS_DEVICES ||
                    proto->SubClass == ITEM_SUBCLASS_EXPLOSIVES);
        }

        virtual string GetName() { return "Engineering"; }
    };

    class OtherTrade : public Trade
    {
    public:
        OtherTrade() : Trade() {}

    public:
        virtual bool Contains(ItemPrototype const* proto)
        {
            return Trade::Contains(proto) &&
                proto->SubClass != ITEM_SUBCLASS_PARTS &&
                proto->SubClass != ITEM_SUBCLASS_DEVICES &&
                proto->SubClass != ITEM_SUBCLASS_EXPLOSIVES;
        }

        virtual string GetName() { return "OtherTrade"; }
    };
};
