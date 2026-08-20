#pragma once

#include <string>

enum PlayerbotSecurityLevel
{
    PLAYERBOT_SECURITY_DENY_ALL = 0,
    PLAYERBOT_SECURITY_TALK = 1,
    PLAYERBOT_SECURITY_INVITE = 2,
    PLAYERBOT_SECURITY_ALLOW_GROUP = 3,
    PLAYERBOT_SECURITY_ALLOW_ALL = 4
};

namespace ai
{
    inline PlayerbotSecurityLevel GetPlayerbotCommandSecurityLevel(std::string const& command)
    {
        // ExternalEventHelper separates a trigger from its parameters on spaces only. Keep
        // this boundary identical so other whitespace cannot gain a lower security tier.
        std::string::size_type end = command.find(' ');
        std::string const name = command.substr(0, end);

        if (name == "who")
        {
            return PLAYERBOT_SECURITY_TALK;
        }
        if (name == "follow" || name == "stay" || name == "attack")
        {
            // These are registered as single-word chat triggers whose remainder is a
            // parameter. A multi-word trigger with one of these prefixes would inherit this
            // tier and therefore requires an explicit security review before registration.
            return PLAYERBOT_SECURITY_ALLOW_GROUP;
        }
        return PLAYERBOT_SECURITY_ALLOW_ALL;
    }

    inline PlayerbotSecurityLevel GetPlayerOwnedBotSecurityLevel(bool isMaster, bool sameSubgroup)
    {
        if (isMaster)
        {
            return PLAYERBOT_SECURITY_ALLOW_ALL;
        }
        if (sameSubgroup)
        {
            return PLAYERBOT_SECURITY_ALLOW_GROUP;
        }
        return PLAYERBOT_SECURITY_TALK;
    }
}
