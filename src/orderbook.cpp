#include "orderbook.hpp"
#include <mutex>
#include <numeric>
#include <ctime>
void Orderbook::PruneGoodForDayOrders()
{
    {
        using namespace std::chrono;
        const auto end = hours(16);

        while (true)
        {
            const auto now = system_clock::now();
            const auto now_c = system_clock::to_time_t(now);
            std::tm now_parts = *std::localtime(&now_c);
            if (now_parts.tm_hour >= end.count())
                now_parts.tm_mday += 1;

            now_parts.tm_hour = end.count();
            now_parts.tm_min = 0;
            now_parts.tm_sec = 0;

            auto next = system_clock::from_time_t(mktime(&now_parts));
            auto till = next - now + milliseconds(100); // to ensure we don't wakeup early

            {
                std::unique_lock ordersLock{ordersMutex_};

                if (shutdown_.load(std::memory_order_acquire) ||
                    shutdownConditionVariable_.wait_for(ordersLock, till) 
                    == std::cv_status::no_timeout // When wakes up due to after shutdown
                    )
                    return;
            }

            OrderIds orderIds;

            {
                std::scoped_lock ordersLock{ordersMutex_};

                for (const auto &[_, entry] : orders_)
                {
                    const auto &[order, _] = entry;

                    if (order->GetOrderType() != OrderType::GoodForDay)
                        continue;

                    orderIds.push_back(order->GetOrderId());
                }
            }

            CancelOrders(orderIds);
        }
    }
}
void Orderbook::CancelOrders(OrderIds orderIds)
{
    std::lock_guard<std::mutex> lock(ordersMutex_);
    for (auto orderId : orderIds)
    {
        CancelOrderInternal(orderId);
    }
}

void Orderbook::CancelOrderInternal(OrderId orderId)
{
    if (!orders_.count(orderId))
        return;
    const auto [orderPointer, orderPointerLocation] = orders_[orderId];
    orders_.erase(orderId);
    if (orderPointer->GetSide() == Side::Buy)
    {
        Price price = orderPointer->GetPrice();
        OrderPointers &orderPointersList = bids_[price];
        orderPointersList.erase(orderPointerLocation);
        if (orderPointersList.empty())
        {
            bids_.erase(price);
        }
    }
    else
    {
        Price price = orderPointer->GetPrice();
        OrderPointers &orderPointersList = asks_[price];
        orderPointersList.erase(orderPointerLocation);
        if (orderPointersList.empty())
        {
            asks_.erase(price);
        }
    }

    OnOrderCancelled(orderPointer);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
    auto &data = data_[price];

    switch (action)
    {
    case LevelData::Action::Add:
        data.count_ += 1;
        data.quantity_ += quantity;
        break;
    case LevelData::Action::Remove:
        data.count_ -= 1;
        data.quantity_ -= quantity;
        break;
    case LevelData::Action::Match:
        data.quantity_ -= quantity;
        break;
    }
    if (data.count_ == 0)
        data_.erase(price);
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;

    std::optional<Price> threshold;

    if (side == Side::Buy)
    {
        const auto [askPrice, _] = *asks_.begin();
        threshold = askPrice;
    }
    else
    {
        const auto [bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
    }

    for (const auto &[levelPrice, levelData] : data_)
    {
        if (threshold.has_value() &&
                (side == Side::Buy && threshold.value() > levelPrice) ||
            (side == Side::Sell && threshold.value() < levelPrice))
            continue;

        if ((side == Side::Buy && levelPrice > price) ||
            (side == Side::Sell && levelPrice < price))
            continue;

        if (quantity <= levelData.quantity_)
            return true;

        quantity -= levelData.quantity_;
    }

    return false;
}

bool Orderbook::CanMatch(Side side, Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty()) // No sellers → can't match
            return false;

        const auto &[bestAsk, _] = *asks_.begin(); // Get lowest sell price (best ask)
        return price >= bestAsk;                   // Can we afford to buy at best ask?
    }

    else
    {
        if (bids_.empty()) // No buyers → can't match
            return false;

        const auto &[bestBid, _] = *bids_.begin(); // Get highest buy price (best bid)
        return price <= bestBid;                   // Is our sell price low enough to match?
    }
}

Trades Orderbook::MatchOrders()
{
    Trades trades;
    trades.reserve(orders_.size());
    while (true)
    {
        if (bids_.empty() || asks_.empty())
            break;

        auto &[bidPrice, bidsPointersList] = *bids_.begin();
        auto &[askPrice, asksPointersList] = *asks_.begin();

        if (bidPrice < askPrice)
            break;

        while (!bidsPointersList.empty() and !asksPointersList.empty())
        {
            auto bidOrder = bidsPointersList.front();
            auto askOrder = asksPointersList.front();

            Quantity quantity = std::min(bidOrder->GetRemainingQuantity(), askOrder->GetRemainingQuantity());

            bidOrder->Fill(quantity);
            askOrder->Fill(quantity);

            if (bidOrder->IsFilled())
            {
                bidsPointersList.pop_front();
                if (bidsPointersList.empty())
                {
                    bids_.erase(bidPrice);
                    data_.erase(bidPrice);
                }
                orders_.erase(bidOrder->GetOrderId());
            }

            if (askOrder->IsFilled())
            {
                asksPointersList.pop_front();
                if (asksPointersList.empty())
                {
                    asks_.erase(askPrice);
                    data_.erase(askPrice);
                }
                orders_.erase(askOrder->GetOrderId());
            }

            trades.emplace_back(Trade{
                TradeInfo{bidOrder->GetOrderId(), bidOrder->GetPrice(), quantity},
                TradeInfo{askOrder->GetOrderId(), askOrder->GetPrice(), quantity}});
        }

        if (!bidsPointersList.empty())
        {
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
        }
    }
}

Orderbook::Orderbook()
{
    ordersPruneThread_ = std::thread([this]
                                     { PruneGoodForDayOrders(); });
}

Orderbook::~Orderbook()
{
    shutdown_.store(true, std::memory_order_release);
    shutdownConditionVariable_.notify_one();
    ordersPruneThread_.join();
}

Trades Orderbook::AddOrder(OrderPointer order)
{
    std::lock_guard ordersLock{ordersMutex_};
    if (orders_.contains(order->GetOrderId()))
    {
        return {};
    }
    if (order->GetOrderType() == OrderType::Market)
    {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto &[worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto &[worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        }
        else
            return {};
    }

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return {};

    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
        return {};

    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
        auto &orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    }
    else
    {
        auto &orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    }

    orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});

    OnOrderAdded(order);

    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
{
    std::lock_guard ordersLock{ordersMutex_};
    CancelOrderInternal(orderId);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
    OrderType orderType;
    {
        std::lock_guard ordersLock{ordersMutex_};

        if (!orders_.contains(order.GetOrderId()))
            return {};

        const auto &[existingOrder, _] = orders_.at(order.GetOrderId());
        orderType = existingOrder->GetOrderType();
    }

    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(orderType));
}

std::size_t Orderbook::Size() const
{
    std::scoped_lock ordersLock{ordersMutex_};
    return orders_.size();
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers &orders)
    {
        return LevelInfo{price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                                                [](Quantity runningSum, const OrderPointer &order) -> Quantity
                                                { return runningSum + order->GetRemainingQuantity(); })};
    };

    for (const auto &[price, orders] : bids_)
        bidInfos.push_back(CreateLevelInfos(price, orders));

    for (const auto &[price, orders] : asks_)
        askInfos.push_back(CreateLevelInfos(price, orders));

    return OrderbookLevelInfos{bidInfos, askInfos};
}
