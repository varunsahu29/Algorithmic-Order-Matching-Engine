#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "usings.hpp"
#include "order.hpp"
#include "order_modify.hpp"
#include "orderbook_level_info.hpp"
#include "trade.hpp"

class Orderbook
{

public:
    Orderbook();
    ~Orderbook();

    // Prevent copying and moving
    Orderbook(const Orderbook &) = delete;      
    void operator=(const Orderbook &) = delete; 
    Orderbook(Orderbook &&) = delete;           
    void operator=(Orderbook &&) = delete;      

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;

private:
    // === Internal Types ===
    struct OrderEntry
    {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    struct LevelData
    {
        Quantity quantity_{};
        Quantity count_{};

        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };
    // === Internal Storage ===
    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    // === Thread Safety ===
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{false};

    // === Matching & Validation ===
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();

    // === Order Management ===
    void PruneGoodForDayOrders();

    // === Orderbook Mutations ===
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    // === State Update Hooks ===
    void OnOrderCancelled(OrderPointer order)
    {
        UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
    };
    void OnOrderAdded(OrderPointer order)
    {
        UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
    };
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
    {
        UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
    };
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);


};
