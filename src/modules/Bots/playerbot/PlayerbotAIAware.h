#pragma once

namespace ai
{
    /**
     * @brief A class that makes the AI aware of the PlayerbotAI instance.
     */
    class PlayerbotAIAware
    {
    public:
        /**
         * @brief Constructor for PlayerbotAIAware.
         * @param ai Pointer to the PlayerbotAI instance.
         */
        PlayerbotAIAware(PlayerbotAI* const ai) : ai(ai) { }

    protected:
        PlayerbotAI* ai; ///< Pointer to the PlayerbotAI instance.
    };
}
