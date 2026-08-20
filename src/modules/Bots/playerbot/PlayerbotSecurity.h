#ifndef _PlayerbotSecurity_H
#define _PlayerbotSecurity_H

#include "PlayerbotSecurityPolicy.h"

using namespace std;

enum DenyReason
{
    PLAYERBOT_DENY_NONE,
    PLAYERBOT_DENY_LOW_LEVEL,
    PLAYERBOT_DENY_GEARSCORE,
    PLAYERBOT_DENY_NOT_YOURS,
    PLAYERBOT_DENY_IS_BOT,
    PLAYERBOT_DENY_OPPOSING,
    PLAYERBOT_DENY_DEAD,
    PLAYERBOT_DENY_FAR,
    PLAYERBOT_DENY_INVITE,
    PLAYERBOT_DENY_FULL_GROUP
};

class PlayerbotSecurity
{
    public:
        PlayerbotSecurity(Player* const bot);

    public:
        PlayerbotSecurityLevel LevelFor(Player* from, DenyReason* reason = NULL, bool ignoreGroup = false);
        bool CheckLevelFor(PlayerbotSecurityLevel level, bool silent, Player* from, bool ignoreGroup = false);

    private:
        Player* const bot;
        uint32 account;
};

#endif
