#include <sstream>
#include <memory>
#include <charconv>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "OrderBook.h"

class OrderBookTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_book.addNewTradeListener([this](const Trade& trade) { m_trades.push_back(trade); });
    }

    const auto& getTrades() const { return m_trades; }

public:
    size_t place()
    {
        auto&& newOrder = Order(m_curOrder);
        size_t id = newOrder.id;
        m_book.placeOrder(newOrder);
        return id;
    }

    OrderBookTest& price(size_t price)
    {
         m_curOrder.price = price;
         return *this;
    }

    OrderBookTest& qty(size_t qty)
    {
        m_curOrder.qty = qty;
        return *this;
    }

    OrderBookTest& id(size_t id)
    {
        m_curOrder.id = id;
        return *this;
    }

    OrderBookTest& buy()
    {
        m_curOrder.side = 'B';
        return *this;
    }

    OrderBookTest& sell()
    {
        m_curOrder.side = 'S';
        return *this;
    }

protected:
    OrderBook m_book;
    std::vector<Trade> m_trades;

private:
    Order m_curOrder{0, 0, 0, 0};
};

TEST_F(OrderBookTest, PlaceOneOrderTest)
{
    auto id = buy().price(100).qty(1000).place();
    ASSERT_EQ(m_book.getBids().size(), 1ul);
    const auto& order = *m_book.getBids().begin();
    ASSERT_EQ(order.id, id);
    ASSERT_EQ(order.price, 100ul);
    ASSERT_EQ(order.qty, 1000ul);
    ASSERT_EQ(order.side, 'B');
    ASSERT_EQ(getTrades().size(), 0ul);
}

TEST_F(OrderBookTest, PlaceTwoDifferentSideOrdersWithoutOrderBookTest)
{
    auto buyId = buy().price(100).qty(1000).place();
    auto sellId = sell().price(1000).qty(100).place();
    ASSERT_EQ(m_book.getBids().size(), 1ul);
    ASSERT_EQ(m_book.getAsks().size(), 1ul);

    const auto& buyOrder = *m_book.getBids().begin();
    ASSERT_EQ(buyOrder.id, buyId);
    ASSERT_EQ(buyOrder.qty, 1000ul);
    ASSERT_EQ(buyOrder.price, 100ul);
    ASSERT_EQ(buyOrder.side, 'B');

    const auto& sellOrder = *m_book.getAsks().begin();
    ASSERT_EQ(sellOrder.id, sellId);
    ASSERT_EQ(sellOrder.qty, 100ul);
    ASSERT_EQ(sellOrder.price, 1000ul);
    ASSERT_EQ(sellOrder.side, 'S');

    ASSERT_EQ(getTrades().size(), 0ul);
}

TEST_F(OrderBookTest, PlaceTwoOrderWithBothFullyTradedWithSamePriceTest)
{
    auto buyId = buy().price(100).qty(1000).place();
    auto sellId = sell().price(100).qty(1000).place();
    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 1ul);
    ASSERT_EQ(trades.front().initOrderId, sellId);
    ASSERT_EQ(trades.front().matchOrderId, buyId);
    ASSERT_EQ(trades.front().executedQty, 1000ul);
    ASSERT_EQ(trades.front().executedPrice, 100ul);

    ASSERT_EQ(m_book.getBids().size(), 0ul);
    ASSERT_EQ(m_book.getAsks().size(), 0ul);
}

TEST_F(OrderBookTest, PlaceTwoOrderWithBothFullyTradedWithDifferentPriceTest)
{
    auto buyId = buy().price(1000).qty(1000).place();
    auto sellId = sell().price(100).qty(1000).place();
    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 1ul);
    ASSERT_EQ(trades.front().initOrderId, sellId);
    ASSERT_EQ(trades.front().matchOrderId, buyId);
    ASSERT_EQ(trades.front().executedQty, 1000ul);
    ASSERT_EQ(trades.front().executedPrice, 1000ul);

    ASSERT_EQ(m_book.getBids().size(), 0ul);
    ASSERT_EQ(m_book.getAsks().size(), 0ul);
}

TEST_F(OrderBookTest, PlaceTwoOrdersOnePartiallyTradedWithSamePriceTest)
{
    auto buyId = buy().price(100).qty(10000).place();
    auto sellId = sell().price(100).qty(1000).place();

    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 1ul);
    ASSERT_EQ(trades.front().initOrderId, sellId);
    ASSERT_EQ(trades.front().matchOrderId, buyId);
    ASSERT_EQ(trades.front().executedQty, 1000ul);
    ASSERT_EQ(trades.front().executedPrice, 100ul);

    ASSERT_EQ(m_book.getBids().size(), 1ul);
    ASSERT_EQ(m_book.getBids().begin()->qty, 9000ul);

    ASSERT_EQ(m_book.getAsks().size(), 0ul);
}

TEST_F(OrderBookTest, PlaceTwoOrdersOnePartiallyTradedWithDifferentPriceTest)
{
    auto sellId = sell().price(1).qty(10000).place();
    auto buyId = buy().price(100).qty(1000).place();

    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 1ul);
    ASSERT_EQ(trades.front().initOrderId, buyId);
    ASSERT_EQ(trades.front().matchOrderId, sellId);
    ASSERT_EQ(trades.front().executedQty, 1000ul);
    ASSERT_EQ(trades.front().executedPrice, 1ul);

    ASSERT_EQ(m_book.getBids().size(), 0ul);

    ASSERT_EQ(m_book.getAsks().size(), 1ul);
    ASSERT_EQ(m_book.getAsks().begin()->qty, 9000ul);
}

TEST_F(OrderBookTest, PlaceNumberOfOppositeOrdersWithoutTradeTest)
{
    std::vector<size_t> sellIds, buyIds;
    for (size_t i = 0; i < 1000; ++i)
    {
        sellIds.push_back(sell().price(10000 + i).qty(10000).place());
        buyIds.push_back(buy().price(i).qty(1000).place());
    }

    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 0ul);

    std::sort(sellIds.begin(), sellIds.end());
    std::sort(buyIds.begin(), buyIds.end());

    std::vector<size_t> bidsIds;
    std::vector<size_t> askIds;

    for (const auto& order : m_book.getBids())
        bidsIds.push_back(order.id);

    for (const auto& order : m_book.getAsks())
        askIds.push_back(order.id);

    std::sort(askIds.begin(), askIds.end());
    std::sort(bidsIds.begin(), bidsIds.end());

    ASSERT_EQ(bidsIds, buyIds);
    ASSERT_EQ(askIds, sellIds);
}

TEST_F(OrderBookTest, PlaceNumberOfOppositeOrdersWithAllFullyTradedTest)
{
    for (size_t i = 1; i <= 1000; ++i)
    {
        size_t sellId = sell().price(1).qty(1).place();
        size_t buyId = buy().price(1).qty(1).place();
        const auto& trades = getTrades();
        ASSERT_EQ(trades.size(), i);
        ASSERT_EQ(trades.back().initOrderId, buyId);
        ASSERT_EQ(trades.back().matchOrderId, sellId);
        ASSERT_EQ(trades.back().executedPrice, 1ul);
        ASSERT_EQ(trades.back().executedQty, 1ul);
    }
}

TEST_F(OrderBookTest, PlaceFewOrdersWithFullAndPartlyTradesTest)
{
    auto buyId1 = buy().price(100).qty(100).place();
    auto buyId2 = buy().price(200).qty(200).place();
    auto buyId3 = buy().price(300).qty(300).place();

    auto sellId1 = sell().price(50).qty(100).place();
    const auto& trades = getTrades();
    ASSERT_EQ(trades.size(), 1ul);
    ASSERT_EQ(trades.back().initOrderId, sellId1);
    ASSERT_EQ(trades.back().matchOrderId, buyId3);
    ASSERT_EQ(trades.back().executedPrice, 300ul);
    ASSERT_EQ(trades.back().executedQty, 100ul);

    ASSERT_EQ(m_book.getAsks().size(), 0ul);
    ASSERT_EQ(m_book.getBids().size(), 3ul);
    ASSERT_EQ(m_book.getBids().begin()->qty, 200ul);

    auto sellId2 = sell().price(50).qty(250).place();
    ASSERT_EQ(trades.size(), 3ul);
    ASSERT_EQ(trades[1].initOrderId, sellId2);
    ASSERT_EQ(trades[1].matchOrderId, buyId3);
    ASSERT_EQ(trades[1].executedPrice, 300ul);
    ASSERT_EQ(trades[1].executedQty, 200ul);
    ASSERT_EQ(trades[2].initOrderId, sellId2);
    ASSERT_EQ(trades[2].matchOrderId, buyId2);
    ASSERT_EQ(trades[2].executedPrice, 200ul);
    ASSERT_EQ(trades[2].executedQty, 50ul);

    ASSERT_EQ(m_book.getAsks().size(), 0ul);
    ASSERT_EQ(m_book.getBids().size(), 2ul);
    ASSERT_EQ(m_book.getBids().begin()->qty, 150ul);

    auto sellId3 = sell().price(50).qty(300).place();
    ASSERT_EQ(trades.size(), 5ul);
    ASSERT_EQ(trades[3].initOrderId, sellId3);
    ASSERT_EQ(trades[3].matchOrderId, buyId2);
    ASSERT_EQ(trades[3].executedPrice, 200ul);
    ASSERT_EQ(trades[3].executedQty, 150ul);
    ASSERT_EQ(trades[4].initOrderId, sellId3);
    ASSERT_EQ(trades[4].matchOrderId, buyId1);
    ASSERT_EQ(trades[4].executedPrice, 100ul);
    ASSERT_EQ(trades[4].executedQty, 100ul);

    ASSERT_EQ(m_book.getAsks().size(), 1ul);
    ASSERT_EQ(m_book.getBids().size(), 0ul);
    ASSERT_EQ(m_book.getAsks().begin()->qty, 50ul);
}











