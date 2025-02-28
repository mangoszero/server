#pragma once

namespace ai
{
    /**
     * @brief Represents an event in the AI system
     */
    class Event
    {
    public:
        /**
         * @brief Copy constructor
         *
         * @param other The other event to copy from
         */
        Event(Event const& other)
        {
            source = other.source;
            param = other.param;
            packet = other.packet;
            owner = other.owner;
        }

        /**
         * @brief Default constructor
         */
        Event() : owner(nullptr) {} // Initialize owner to nullptr

        /**
         * @brief Constructor with source
         *
         * @param source The source of the event
         */
        Event(string source) : source(source), owner(nullptr) {} // Initialize owner to nullptr

        /**
         * @brief Constructor with source, parameter, and owner
         *
         * @param source The source of the event
         * @param param The parameter of the event
         * @param owner The owner of the event
         */
        Event(string source, string param, Player* owner = NULL) : source(source), param(param), owner(owner) {}

        /**
         * @brief Constructor with source, packet, and owner
         *
         * @param source The source of the event
         * @param packet The packet associated with the event
         * @param owner The owner of the event
         */
        Event(string source, WorldPacket &packet, Player* owner = NULL) : source(source), packet(packet), owner(owner) {}

        /**
         * @brief Destructor
         */
        virtual ~Event() {}

    public:
        /**
         * @brief Get the source of the event
         *
         * @return string The source of the event
         */
        string getSource() { return source; }

        /**
         * @brief Get the parameter of the event
         *
         * @return string The parameter of the event
         */
        string getParam() { return param; }

        /**
         * @brief Get the packet associated with the event
         *
         * @return WorldPacket& The packet associated with the event
         */
        WorldPacket& getPacket() { return packet; }

        /**
         * @brief Get the object associated with the event
         *
         * @return ObjectGuid The object associated with the event
         */
        ObjectGuid getObject();

        /**
         * @brief Get the owner of the event
         *
         * @return Player* The owner of the event
         */
        Player* getOwner() { return owner; }

        /**
         * @brief Check if the event is valid
         *
         * @return true if the event is valid, false otherwise
         */
        bool operator! () const { return source.empty(); }

    protected:
        string source; /**< The source of the event */
        string param; /**< The parameter of the event */
        WorldPacket packet; /**< The packet associated with the event */
        ObjectGuid object; /**< The object associated with the event */
        Player* owner; /**< The owner of the event */
    };
}
