#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"

using namespace ai;
using namespace std;

/**
 * @brief Constructor for PlayerbotAIBase.
 * Initializes the next AI check delay to 0.
 */
PlayerbotAIBase::PlayerbotAIBase() : nextAICheckDelay(0)
{
}

/**
 * @brief Updates the AI.
 * @param elapsed The time elapsed since the last update.
 */
void PlayerbotAIBase::UpdateAI(uint32 elapsed)
{
    // Decrease the next AI check delay by the elapsed time
    if (nextAICheckDelay > elapsed)
    {
        nextAICheckDelay -= elapsed;
    }
    else
    {
        nextAICheckDelay = 0;
    }

    // Check if the AI can be updated
    if (!CanUpdateAI())
    {
        return;
    }

    // Update the AI internal state
    UpdateAIInternal(elapsed);
    // Yield the current thread
    YieldThread();
}

/**
 * @brief Sets the delay for the next AI check.
 * @param delay The delay in milliseconds.
 */
void PlayerbotAIBase::SetNextCheckDelay(const uint32 delay)
{
    nextAICheckDelay = delay;

    // Log if the new delay is greater than the global cooldown
    if (nextAICheckDelay > sPlayerbotAIConfig.globalCoolDown)
    {
        sLog.outDebug("set next check delay: %d", nextAICheckDelay);
    }
}

/**
 * @brief Increases the delay for the next AI check.
 * @param delay The delay in milliseconds to add to the current delay.
 */
void PlayerbotAIBase::IncreaseNextCheckDelay(uint32 delay)
{
    nextAICheckDelay += delay;

    // Log if the new delay is greater than the global cooldown
    if (nextAICheckDelay > sPlayerbotAIConfig.globalCoolDown)
    {
        sLog.outDebug("increase next check delay: %d", nextAICheckDelay);
    }
}

/**
 * @brief Checks if the AI can be updated.
 * @return True if the AI can be updated, false otherwise.
 */
bool PlayerbotAIBase::CanUpdateAI() const
{
    return nextAICheckDelay < 100;
}

/**
 * @brief Yields the current thread.
 * This function can be used to pause the execution of the current thread.
 */
void PlayerbotAIBase::YieldThread()
{
    if (nextAICheckDelay < sPlayerbotAIConfig.reactDelay)
    {
        nextAICheckDelay = sPlayerbotAIConfig.reactDelay;
    }
}
