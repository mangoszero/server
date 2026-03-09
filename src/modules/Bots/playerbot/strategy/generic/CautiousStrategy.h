#pragma once

namespace ai
{
    class CautiousStrategy : public Strategy
    {
    public:
        CautiousStrategy(PlayerbotAI* ai) : Strategy(ai) {}
        virtual string getName() { return "cautious"; }
    };
}
