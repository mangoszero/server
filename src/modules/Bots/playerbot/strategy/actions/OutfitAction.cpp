#include "botpch.h"
#include "../../playerbot.h"
#include "OutfitAction.h"
#include "../values/OutfitListValue.h"

using namespace ai;


bool OutfitAction::Execute(Event event)
{
    string param = event.getParam();

    if (param == "?")
    {
        List();
        ai->TellMaster("outfit <name> +[item] to add items");
        ai->TellMaster("outfit <name> -[item] to remove items");
        ai->TellMaster("outfit <name> equip to equip items");
    }
    else
    {
        string name = parseName(param);
        ItemIds items = parseItems(param);
        if (!name.empty())
        {
            Save(name, items);
            ostringstream out;
            out << "Setting outfit " << name << " as " << param;
            ai->TellMaster(out);
            return true;
        }

        items = chat->parseItems(param);

        int space = param.find(" ");
        if (space == -1)
        {
            return false;
        }

        name = param.substr(0, space);
        ItemIds outfit = Find(name);
        string command = param.substr(space + 1);
        if (command == "equip")
        {
            ostringstream out;
            out << "Equipping outfit " << name;
            ai->TellMaster(out);
            EquipItems(outfit);
            return true;
        }
        else if (command == "reset")
        {
            ostringstream out;
            out << "Resetting outfit " << name;
            ai->TellMaster(out);
            Save(name, ItemIds());
            return true;
        }
        else if (command == "update")
        {
            ostringstream out;
            out << "Updating to current items outfit " << name;
            ai->TellMaster(out);
            Update(name);
            return true;
        }

        bool remove = param.size() > 1 && param.substr(space + 1, 1) == "-";
        for (ItemIds::iterator i = items.begin(); i != items.end(); i++)
        {
            uint32 itemid = *i;
            ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(*i);
            ostringstream out;
            out << chat->formatItem(proto);
            if (remove)
            {
                set<uint32>::iterator j = outfit.find(itemid);
                if (j != outfit.end())
                {
                    outfit.erase(j);
                }

                out << " removed from ";
            }
            else
            {
                outfit.insert(itemid);
                out << " added to ";
            }
            out << name;
            ai->TellMaster(out.str());
        }
        Save(name, outfit);
    }

    return true;
}

void OutfitAction::Save(string name, ItemIds items)
{
    list<string>& outfits = AI_VALUE(list<string>&, "outfit list");
    for (list<string>::iterator i = outfits.begin(); i != outfits.end(); ++i)
    {
        string outfit = *i;
        if (name == parseName(outfit))
        {
            outfits.erase(i);
            break;
        }
    }

    if (items.empty()) return;

    ostringstream out;
    out << name << "=";
    bool first = true;
    for (ItemIds::iterator i = items.begin(); i != items.end(); i++)
    {
        if (first) first = false; else out << ",";
        {
            out << *i;
        }
    }
    outfits.push_back(out.str());
}

ItemIds OutfitAction::Find(string name)
{
    list<string>& outfits = AI_VALUE(list<string>&, "outfit list");
    for (list<string>::iterator i = outfits.begin(); i != outfits.end(); ++i)
    {
        string outfit = *i;
        if (name == parseName(outfit))
        {
            return parseItems(outfit);
        }
    }
    return set<uint32>();
}


void OutfitAction::List()
{
    list<string>& outfits = AI_VALUE(list<string>&, "outfit list");
    for (list<string>::iterator i = outfits.begin(); i != outfits.end(); ++i)
    {
        string outfit = *i;
        string name = parseName(outfit);
        ItemIds items = parseItems(outfit);

        ostringstream out;
        out << name << ": ";
        for (ItemIds::iterator j = items.begin(); j != items.end(); ++j)
        {
            ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(*j);
            if (proto)
            {
                out << chat->formatItem(proto) << " ";
            }
        }
        ai->TellMaster(out);
    }
}

string OutfitAction::parseName(string outfit)
{
    int pos = outfit.find("=");
    if (pos == -1) return "";
    {
        return outfit.substr(0, pos);
    }
}

ItemIds OutfitAction::parseItems(string &text)
{
    ItemIds itemIds;

    uint8 pos = text.find("=") + 1;
    while (pos < text.size())
    {
        int endPos = text.find(',', pos);
        if (endPos == -1)
        {
            endPos = text.size();
        }

        string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos + 1;
        if (id)
        {
            itemIds.insert(id);
        }
    }

    return itemIds;
}

void OutfitAction::Update(string name)
{
    ListItemsVisitor visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_EQUIP);

    ItemIds items;
    for (map<uint32, int>::iterator i = visitor.items.begin(); i != visitor.items.end(); ++i)
    {
        items.insert(i->first);
    }

    Save(name, items);
}
