# pragma once

#include <expected>
#include <string>
#include <format>
#include <memory>
#include <list>

#include "domain_types.hpp"
#include "order_type.hpp"
#include "side.hpp"
#include "constants.hpp"
namespace osbornex {
    
    class Order
    {
    public:
        Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_ { side }
        , price_ { price }
        , initialQuantity_ { quantity }
        , remainingQuantity_ { quantity }
        { }

        Order(OrderId orderId, Side side, Quantity quantity)
            : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
        {}
    
        OrderType GetOrderType() const { return orderType_; }
        OrderId GetOrderId() const { return orderId_; }
        Side GetSide() const { return side_;  }
        Price GetPrice() const { return price_; }
        Quantity GetInitialQuantity() const{ return initialQuantity_; }
        Quantity GetRemainingQuantity() const { return remainingQuantity_; }
        Quantity GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }
        bool IsFilled() const { return remainingQuantity_ == Quantity{0};  }
    
        /*[[nodiscard]] std::expected<void, std::string>*/
        void Fill(Quantity quantity)
        {
            if (quantity > GetRemainingQuantity())
                return; /*std::unexpected(
                    std::format("OrderID [{}] cannot be filled as the fill quantity [{}] is greater than the remaining quantity [{}]",
                    GetOrderId(), quantity, GetRemainingQuantity()) 
                );*/
    
            remainingQuantity_ -= quantity;
        }

        /*[[nodiscard]] std::expected<void, std::string>*/
        void ToGoodTillCancel(Price price)
        {
            if (GetOrderType() != OrderType::Market)
                throw std::logic_error(std::format("Order ({}) cannot have its price adjusted, only market orders can.", GetOrderId()));

            price_ = price;
            orderType_ = OrderType::GoodTillCancel;
        }
    
    
    private:
        OrderType orderType_;
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity initialQuantity_;
        Quantity remainingQuantity_;
    };

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

} // namespace osbornex