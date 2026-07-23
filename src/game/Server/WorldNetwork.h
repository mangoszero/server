#ifndef MANGOS_H_WORLDNETWORK
#define MANGOS_H_WORLDNETWORK

#include "Listener.h"
#include "Policies/Singleton.h"
#include "WorldGateway.h"

#include <string>

class WorldNetwork : public MaNGOS::Singleton<WorldNetwork>
{
    friend class MaNGOS::Singleton<WorldNetwork>;

public:
    bool Start(uint16 port, std::string const& bindIp);
    void Stop();
    uint32 GetOpenConnectionCount() const;

private:
    WorldNetwork();
    ~WorldNetwork();

    WorldGateway m_gateway;
    proto::Listener m_listener;
    bool m_started = false;
};

#define sWorldNetwork MaNGOS::Singleton<WorldNetwork>::Instance()

#endif
