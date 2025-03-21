#pragma once

#include "trade_info.hpp"

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
    { }

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;
