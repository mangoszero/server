#include "Category.h"
#include "ItemBag.h"
#include "ConsumableCategory.h"
#include "TradeCategory.h"
#include "AhBotConfig.h"
#include "DBCStructure.h"
#include "Log.h"
#include "QueryResult.h"
#include "DatabaseEnv.h"
#include "SQLStorage.h"
#include "DBCStore.h"
#include "SQLStorages.h"
#include "AuctionHouseMgr.h"
#include "ObjectMgr.h"

using namespace ahbot;
char * strstri (const char* str1, const char* str2);

CategoryList CategoryList::instance;

CategoryList::CategoryList()
{
    Add(new Equip());
    Add(new ahbot::Quest());
    Add(new Quiver());
    Add(new Projectile());

    Add(new Recipe());
    Add(new Container());

    Add(new Reagent());
    Add(new Alchemy());
    Add(new Scroll());
    Add(new Food());
    Add(new Bandage());

    Add(new Engineering());

    Add(new OtherConsumable());
    Add(new OtherTrade());
    Add(new Other());
}

void CategoryList::Add(Category* category)
{
    for (uint32 quality = ITEM_QUALITY_NORMAL; quality <= ITEM_QUALITY_EPIC; ++quality)
    {
        categories.push_back(new QualityCategoryWrapper(category, quality));
    }
}

CategoryList::~CategoryList()
{
    for (vector<Category*>::const_iterator i = categories.begin(); i != categories.end(); ++i)
    {
        delete *i;
    }
}

ItemBag::ItemBag()
{
    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        content[CategoryList::instance[i]] = vector<uint32>();
    }
}

void ItemBag::Init(bool silent)
{
    if (silent)
    {
        Load();
        return;
    }

    sLog.outString("Loading/Scanning %s...", GetName().c_str());

    Load();

    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        Shuffle(content[category]);
        sLog.outString("loaded %d %s items", content[category].size(), category->GetDisplayName().c_str());
    }
}

int32 ItemBag::GetCount(Category* category, uint32 item)
{
    uint32 count = 0;

    vector<uint32>& items = content[category];
    for (vector<uint32>::iterator i = items.begin(); i != items.end(); ++i)
    {
        if (*i == item)
        {
            count++;
        }
    }

    return count;
}

bool ItemBag::Add(ItemPrototype const* proto)
{
    if (!proto ||
        proto->Bonding == BIND_WHEN_PICKED_UP ||
        proto->Bonding == BIND_QUEST_ITEM)
        return false;

    if (proto->RequiredLevel > sAhBotConfig.maxRequiredLevel || proto->ItemLevel > sAhBotConfig.maxItemLevel)
    {
        return false;
    }

    if (proto->Duration & 0x80000000)
    {
        return false;
    }

    if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
    {
        return false;
    }

    if (strstri(proto->Name1, "qa") || strstri(proto->Name1, "test") || strstri(proto->Name1, "deprecated"))
    {
        return false;
    }

    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        if (CategoryList::instance[i]->Contains(proto))
        {
            content[CategoryList::instance[i]].push_back(proto->ItemId);
            return true;
        }
    }

    return false;
}

void AvailableItemsBag::Load()
{
    set<uint32> vendorItems;

      QueryResult* results = WorldDatabase.PQuery("SELECT `item` FROM `npc_vendor` WHERE `maxcount` = 0");
      if (results != NULL)
      {
          do
          {
              Field* fields = results->Fetch();
              vendorItems.insert(fields[0].GetUInt32());
          } while (results->NextRow());

          delete results;
      }

      for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
      {
          if (vendorItems.find(itemId) != vendorItems.end())
          {
              continue;
          }

        Add(sObjectMgr.GetItemPrototype(itemId));
    }

}

void InAuctionItemsBag::Load()
{
    AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionId);
    if(!ahEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    AuctionHouseObject::AuctionEntryMap const& auctionEntryMap = auctionHouse->GetAuctions();
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itr->second->itemTemplate);
        if (!proto)
        {
            continue;
        }

        Add(proto);
    }
}

string InAuctionItemsBag::GetName()
{
    ostringstream out; out << "auction house " << auctionId;
    return out.str();
}
