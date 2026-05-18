#pragma once
#include "Action.h"
#include "Event.h"
#include "../PlayerbotAIAware.h"
#include "AiObject.h"

namespace ai
{
    /**
     * @brief Base class for untyped values.
     */
    class UntypedValue : public AiNamedObject
    {
    public:
        UntypedValue(PlayerbotAI* ai, string name) : AiNamedObject(ai, name) {}
        virtual void Update() {}
        virtual void Reset() {}
        virtual string Format() { return "?"; }
    };

    /**
     * @brief Template class for typed values.
     *
     * @tparam T The type of the value.
     */
    template<class T>
    class Value
    {
    public:
        virtual T Get() = 0;
        virtual void Set(T value) = 0;
        operator T() { return Get(); }
    };

    /**
     * @brief Template class for calculated values.
     *
     * @tparam T The type of the value.
     */
    template<class T>
    class CalculatedValue : public UntypedValue, public Value<T>
    {
    public:
        /**
         * @brief Construct a new Calculated Value object
         *
         * @param ai Pointer to the PlayerbotAI instance.
         * @param name The name of the value.
         * @param checkInterval The interval at which the value is checked.
         */
        CalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) : UntypedValue(ai, name),
            checkInterval(checkInterval), ticksElapsed(checkInterval)
        { }

        /**
         * @brief Destroy the Calculated Value object
         */
        virtual ~CalculatedValue() {}

    public:
        /**
         * @brief Get the calculated value.
         *
         * @return T The calculated value.
         */
        virtual T Get()
        {
            if (ticksElapsed >= checkInterval)
            {
                ticksElapsed = 0;
                value = Calculate();
            }
            return value;
        }

        /**
         * @brief Set the value.
         *
         * @param value The value to set.
         */
        virtual void Set(T value) { this->value = value; }
        virtual void Update()
        {
            if (ticksElapsed < checkInterval)
            {
                ticksElapsed++;
            }
        }

    protected:
        /**
         * @brief Calculate the value.
         *
         * @return T The calculated value.
         */
        virtual T Calculate() = 0;

    protected:
        int checkInterval; ///< The interval at which the value is checked.
        int ticksElapsed;
        T value; ///< The cached value.
    };


    /**
     * @brief Class for calculated uint8 values.
     */
    class Uint8CalculatedValue : public CalculatedValue<uint8>
    {
    public:
        Uint8CalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<uint8>(ai, name, checkInterval) {}

        virtual string Format()
        {
            ostringstream out; out << (int)Calculate();
            return out.str();
        }
    };

    /**
     * @brief Class for calculated uint32 values.
     */
    class Uint32CalculatedValue : public CalculatedValue<uint32>
    {
    public:
        Uint32CalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<uint32>(ai, name, checkInterval) {}

        virtual string Format()
        {
            ostringstream out; out << (int)Calculate();
            return out.str();
        }
    };

    /**
     * @brief Class for calculated float values.
     */
    class FloatCalculatedValue : public CalculatedValue<float>
    {
    public:
        FloatCalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<float>(ai, name, checkInterval) {}

        virtual string Format()
        {
            ostringstream out; out << Calculate();
            return out.str();
        }
    };

    /**
     * @brief Class for calculated bool values.
     */
    class BoolCalculatedValue : public CalculatedValue<bool>
    {
    public:
        BoolCalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<bool>(ai, name, checkInterval) {}

        virtual string Format()
        {
            return Calculate() ? "true" : "false";
        }
    };

    /**
     * @brief Class for calculated Unit* values.
     */
    class UnitCalculatedValue : public CalculatedValue<Unit*>
    {
    public:
        UnitCalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<Unit*>(ai, name, checkInterval) {}

        virtual string Format()
        {
            Unit* unit = Calculate();
            return unit ? unit->GetName() : "<none>";
        }
    };

    /**
     * @brief Class for calculated list<ObjectGuid> values.
     */
    class ObjectGuidListCalculatedValue : public CalculatedValue<list<ObjectGuid> >
    {
    public:
        ObjectGuidListCalculatedValue(PlayerbotAI* ai, string name = "value", int checkInterval = 1) :
            CalculatedValue<list<ObjectGuid> >(ai, name, checkInterval) {}

        virtual string Format()
        {
            ostringstream out; out << "{";
            list<ObjectGuid> guids = Calculate();
            for (list<ObjectGuid>::iterator i = guids.begin(); i != guids.end(); ++i)
            {
                ObjectGuid guid = *i;
                out << guid.GetRawValue() << ",";
            }
            out << "}";
            return out.str();
        }
    };

    /**
     * @brief Template class for manually set values.
     *
     * @tparam T The type of the value.
     */
    template<class T>
    class ManualSetValue : public UntypedValue, public Value<T>
    {
    public:
        ManualSetValue(PlayerbotAI* ai, T defaultValue, string name = "value") :
            UntypedValue(ai, name), value(defaultValue), defaultValue(defaultValue) {}
        virtual ~ManualSetValue() {}

    public:
        virtual T Get() { return value; }
        virtual void Set(T value) { this->value = value; }
        virtual void Update() { }
        virtual void Reset() { value = defaultValue; }

    protected:
        T value; ///< The current value.
        T defaultValue; ///< The default value.
    };

    /**
     * @brief Class for manually set Unit* values.
     */
    class UnitManualSetValue : public ManualSetValue<Unit*>
    {
    public:
        UnitManualSetValue(PlayerbotAI* ai, Unit* defaultValue, string name = "value") :
            ManualSetValue<Unit*>(ai, defaultValue, name) {}

        virtual string Format()
        {
            Unit* unit = Get();
            return unit ? unit->GetName() : "<none>";
        }
    };
}
