#include "botpch.h"
#include "../../playerbot.h"
#include "SayAction.h"

using namespace ai;

// Static member variables to store speech strings and their probabilities
map<string, vector<string> > SayAction::stringTable;
map<string, uint32 > SayAction::probabilityTable;

// Constructor for SayAction, initializes the action with the name "say"
SayAction::SayAction(PlayerbotAI* ai) : Action(ai, "say"), Qualified()
{
}

// Helper function to replace all occurrences of a substring with another substring
static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
    {
        return;
    }
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

// Executes the SayAction, making the bot say something based on predefined strings and probabilities
bool SayAction::Execute(Event event)
{
    // Load speech strings from the database if not already loaded
    if (stringTable.empty())
    {
        QueryResult* results = CharacterDatabase.PQuery("SELECT name, text, type FROM ai_playerbot_speech");
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                string name = fields[0].GetString();
                string text = fields[1].GetString();
                string type = fields[2].GetString();

                if (type == "yell") text = "/y " + text;
                {
                    stringTable[name].push_back(text);
                }
            } while (results->NextRow());
            delete results;
        }
    }

    // Load speech probabilities from the database if not already loaded
    if (probabilityTable.empty())
    {
        QueryResult* results = CharacterDatabase.PQuery("SELECT name, probability FROM ai_playerbot_speech_probability");
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                string name = fields[0].GetString();
                uint32 probability = fields[1].GetUInt32();

                probabilityTable[name] = probability;
            } while (results->NextRow());
            delete results;
        }
    }

    // Get the list of possible strings for the current qualifier
    vector<string> &strings = stringTable[qualifier];
    if (strings.empty()) return false;

    // Update the last said time for the current qualifier
    time_t lastSaid = AI_VALUE2(time_t, "last said", qualifier);
    ai->GetAiObjectContext()->GetValue<time_t>("last said", qualifier)->Set(time(0) + urand(1, 60));

    // Get the probability for the current qualifier
    uint32 probability = probabilityTable[qualifier];
    if (!probability) probability = 100;
    {
        if (urand(0, 100) >= probability) return false;
    }

    // Select a random string from the list
    uint32 idx = urand(0, strings.size() - 1);
    string text = strings[idx];

    // Replace placeholders in the string with actual values
    Unit* target = AI_VALUE(Unit*, "tank target");
    if (!target) target = AI_VALUE(Unit*, "current target");
    {
        if (target) replaceAll(text, "<target>", target->GetName());
    }

    replaceAll(text, "<randomfaction>", IsAlliance(bot->getRace()) ? "Alliance" : "Horde");

    if (bot->GetMap())
    {
        const TerrainInfo * terrain = bot->GetMap()->GetTerrain();
        if (terrain)
        {
            uint32 areaId = terrain->GetAreaId(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            if (areaId)
            {
                AreaTableEntry const* area = sAreaStore.LookupEntry(areaId);
                if (area)
                {
                    replaceAll(text, "<subzone>", area->area_name[0]);
                }
            }
        }
    }

    // Make the bot say or yell the text
    if (text.find("/y ") == 0)
    {
        bot->Yell(text.substr(3), LANG_UNIVERSAL);
    }
    else
    {
        bot->Say(text, LANG_UNIVERSAL);
    }

    return true;
}

// Checks if the SayAction is useful, based on the time since the last message
bool SayAction::isUseful()
{
    time_t lastSaid = AI_VALUE2(time_t, "last said", qualifier);
    return (time(0) - lastSaid) > 30;
}
