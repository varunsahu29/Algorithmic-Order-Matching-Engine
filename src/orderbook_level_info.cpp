#include "orderbook_level_info.hpp"

OrderbookLevelInfos::OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{ bids }
        , asks_{ asks }
    { }