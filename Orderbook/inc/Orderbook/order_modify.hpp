# pragma once

#include "order.hpp"

namespace osbornex {

    class OrderModify
    {
    public:
        OrderModify(OrderId orderId, Side side, Price price, Quantity quantity )
            : orderId_{ orderId }
            , side_{ side }
            , price_{ price }
            , quantity_ { quantity }
        { }
    
        OrderId GetOrderId() const { return orderId_; }
        Side GetSide() const { return side_; }
        Price GetPrice() const { return price_; }
        Quantity GetQuantity() const { return quantity_; }
    
        OrderPointer ToOrderPointer(OrderType orderType)
        {
            return std::make_shared<Order>(orderType, orderId_, side_, price_, quantity_);
        }
    
    private:
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity quantity_;
    };

} // namespace osbornex