#include "order.hpp"

Order::Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) : orderType_{orderType}, orderId_{orderId}, side_{side}, price_{price}, initialQuantity_{quantity}, remainingQuantity_{quantity}
{
}

Order::Order(OrderId orderId, Side side, Quantity quantity) : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
{
}

void Order::Fill(Quantity quantity)
{
    if (quantity > GetRemainingQuantity())
        throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));

    remainingQuantity_ -= quantity;
}

void Order::ToGoodTillCancel(Price price)
{
    if (GetOrderType() != OrderType::Market)
        throw std::logic_error(std::format("Order ({}) cannot have its price adjusted, only market orders can.", GetOrderId()));

    price_ = price;
    orderType_ = OrderType::GoodTillCancel;
}
