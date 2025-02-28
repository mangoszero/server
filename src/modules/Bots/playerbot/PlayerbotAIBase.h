#pragma once

class Player;
class PlayerbotMgr;
class ChatHandler;

using namespace std;

/**
 * @brief Base class for Playerbot AI.
 * This class provides the basic functionality for managing the AI of player bots.
 */
class PlayerbotAIBase
{
public:
    /**
     * @brief Constructor for PlayerbotAIBase.
     * Initializes the next AI check delay to 0.
     */
    PlayerbotAIBase();

public:
    /**
     * @brief Checks if the AI can be updated.
     * @return True if the AI can be updated, false otherwise.
     */
    bool CanUpdateAI() const;

    /**
     * @brief Sets the delay for the next AI check.
     * @param delay The delay in milliseconds.
     */
    void SetNextCheckDelay(const uint32 delay);

    /**
     * @brief Increases the delay for the next AI check.
     * @param delay The delay in milliseconds to add to the current delay.
     */
    void IncreaseNextCheckDelay(uint32 delay);

    /**
     * @brief Yields the current thread.
     * This function can be used to pause the execution of the current thread.
     */
    void YieldThread();

    /**
     * @brief Updates the AI.
     * @param elapsed The time elapsed since the last update.
     */
    virtual void UpdateAI(uint32 elapsed);

    /**
     * @brief Updates the AI internal state.
     * This function must be implemented by derived classes to provide specific AI update logic.
     * @param elapsed The time elapsed since the last update.
     */
    virtual void UpdateAIInternal(uint32 elapsed) = 0;

protected:
    uint32 nextAICheckDelay; ///< The delay for the next AI check in milliseconds.
};
