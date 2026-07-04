#pragma once

#include "../Value.h"

namespace ai
{
    class MasterBobberValue : public CalculatedValue<GameObject*>
    {
        public:
            MasterBobberValue(PlayerbotAI* ai) : CalculatedValue<GameObject*>(ai, "master bobber") {}
            virtual GameObject* Calculate()
            {
                Player* master = GetMaster();
                if (!master)
                {
                    return nullptr;
                }

                ObjectGuid channelGuid = master->GetChannelObjectGuid();
                if (!channelGuid)
                {
                    return nullptr;
                }

                return master->GetMap()->GetGameObject(channelGuid);
            }
    };
}
