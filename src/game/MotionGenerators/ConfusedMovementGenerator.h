#ifndef MANGOS_CONFUSEDMOVEMENTGENERATOR_H
#define MANGOS_CONFUSEDMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

/**
 * @brief ConfusedMovementGenerator is a movement generator that makes a unit move in a confused manner.
 */
template<class T>
class ConfusedMovementGenerator
    : public MovementGeneratorMedium< T, ConfusedMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for ConfusedMovementGenerator.
         */
        explicit ConfusedMovementGenerator() : i_nextMoveTime(0), i_x(0.0f), i_y(0.0f), i_z(0.0f) {}

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(T& owner);

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(T& owner);

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(T& owner);

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(T& owner);

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(T& owner, const uint32& diff);

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return CONFUSED_MOTION_TYPE; }

    private:
        TimeTracker i_nextMoveTime; ///< Time tracker for the next move.
        float i_x, i_y, i_z; ///< Coordinates for the next move.
};

#endif // MANGOS_CONFUSEDMOVEMENTGENERATOR_H
