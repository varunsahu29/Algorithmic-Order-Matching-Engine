#pragma once

#include <list>
#include <exception>
#include "order_types.hpp"
#include "side.hpp"
#include "usings.hpp"
#include "constants.hpp"
#include <memory>
#include <format>

class Order
{
private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;

public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity);
    Order(OrderId orderId, Side side, Quantity quantity); // For Market Order

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }

    void Fill(Quantity quantity);
    void ToGoodTillCancel(Price price);
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
