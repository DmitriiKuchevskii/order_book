//
// Created by dmitrii on 2/15/20.
//

#ifndef ORDER_BOOK_ORDERBOOK_H
#define ORDER_BOOK_ORDERBOOK_H

#include <regex>
#include <sstream>
#include <string>
#include <sstream>
#include <set>
#include <unordered_set>

constexpr char BuySide = 'B';
constexpr char SellSide = 'S';
constexpr char PlaceAction = 'A';
constexpr char RemoveAction = 'X';

struct Order
{
    static size_t global_orders_counter;
    char action;
    char side;
    size_t price;
    size_t qty;
    size_t id;
    size_t globalNumber;
};
size_t Order::global_orders_counter = 0;

std::ostream& operator << (std::ostream& os, const Order& order)
{
    return os << "Order=[" << "ID: " << order.id << "; "
                    << "Side: " << (order.side == BuySide ? "Buy" : "Sell") << "; "
                    << "Price: " << order.price << "; "
                    << "Quantity: " << order.qty << "]";
}

inline Order parse_order(const std::string& line)
{
    std::vector<std::string> vals = {
        std::sregex_token_iterator{line.begin(), line.end(), (const std::regex&)std::regex(","), -1}, {}
    };

    return {
          vals[0][0]
        , vals[2][0]
        , std::stoull(vals[4])
        , std::stoull(vals[3])
        , std::stoull(vals[1])
        , Order::global_orders_counter++
    };
}

inline std::optional<Order> get_next_order(std::istream& stream)
{
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty()) continue;

        return parse_order(line);
    }
    return std::nullopt;
}

class OrderBook
{
public:
    void addOrder(const Order& order)
    {
        if (order.action == RemoveAction)
            return removeOrder(order);

        std::cout << "Request to add a new order " << order << "\n";
        order.side == BuySide ? addBid(order) : addAsk(order);
    }

    void print() const
    {
        std::cout << "=================\n";
        std::cout << "ASK";
        size_t prevPrice = -1;
        for (const auto& order: m_asks)
        {
            if (prevPrice != order.price)
                std::cout << "\n" << order.price << ": ";

            std::cout  << order.qty << " ";
            prevPrice = order.price;
        }
        std::cout << "\n------------";
        prevPrice = -1;
        for (const auto& order: m_bids)
        {
            if (prevPrice != order.price)
                std::cout << "\n" << order.price << ": ";

            std::cout  << order.qty << " ";
            prevPrice = order.price;
        }
        std::cout << "\nBID\n=================\n";
    }

private:
    void removeOrder(const Order& order)
    {
        std::cout << "Request to remove an order " << order << ". ";
        auto it = m_del.find(order.id);
        if (it == m_del.end())
        {
            std::cout << "FAILED. Order not found.\n";
            return;
        }

        if (order.side == BuySide)
            m_bids.erase(it->second);
        else
            m_asks.erase(it->second);

        m_del.erase(it);
        std::cout << "OK. Order removed.\n";
    }

    void addBid(const Order& order)
    {
        auto res = m_bids.emplace(order);
        m_del[order.id] = res.first;
        doTradesIfNeed(BuySide);
    }

    void addAsk(const Order& order)
    {
        auto res = m_asks.emplace(order);
        m_del[order.id] = res.first;
        doTradesIfNeed(SellSide);
    }

    void doTradesIfNeed(char initSide)
    {
        if (m_bids.empty() || m_asks.empty())
            return;

        if (m_bids.begin()->price < m_asks.begin()->price)
            return;

        doTrades(initSide);
    }

    void doTrades(char initSide)
    {
        auto& init = initSide == BuySide ? m_bids : m_asks;
        auto initiater = init.extract(init.begin());
        auto& counter = initSide == BuySide ? m_asks : m_bids;
        for (auto it = counter.begin(); it != counter.end();)
        {
            if ((initiater.value().side == BuySide && initiater.value().price < it->price) ||
                (initiater.value().side == SellSide && initiater.value().price > it->price))
            {
                it = init.insert(init.end(), std::move(initiater));
                m_del[it->id] = it;
                return;
            }

            std::cout << "Order "<< initiater.value().id << "(" << (initSide == BuySide ? "Buy" : "Sell")
                      <<  ") has initialized a trade with order "
                      << it->id << "(" << (initSide == BuySide ? "Sell" : "Buy") << ")"
                      << " for " << std::min(initiater.value().qty, it->qty)
                      << " shares at price " << it->price << " USD. ";

            if (initiater.value().qty >= it->qty)
            {
                initiater.value().qty -= it->qty;
                m_del.erase(it->id);
                size_t counterId = it->id;
                it = counter.erase(it);
                if (initiater.value().qty == 0)
                {
                    m_del.erase(initiater.value().id);
                    std::cout << "Both orders have been fully traded.\n";
                    return;
                }
                std::cout << "Order " << initiater.value().id << " has been partially traded (Quantity left: "
                          << initiater.value().qty << "). Order " << counterId << " has been fully traded.\n";
                continue;
            }
            else
            {
                auto tmp = counter.extract(it);
                tmp.value().qty -= initiater.value().qty;
                it = counter.insert(counter.end(), std::move(tmp));
                m_del[it->id] = it;
                m_del.erase(initiater.value().id);
                std::cout << "Order " << initiater.value().id << " has been fully traded. Order "
                          << it->id << " has been partially traded (Quantity left: " << it->qty << ")\n";
                return;
            }
        }
    }

private:
    std::set<Order, std::function<bool(const Order&, const Order&)>> m_bids
    {
        [](const Order& first, const Order& second)
        {
            if (first.price == second.price)
                return first.globalNumber < second.globalNumber;
            return first.price > second.price;
        }
    };
    std::set<Order, std::function<bool(const Order&, const Order&)>> m_asks
    {
        [](const Order& first, const Order& second)
        {
            if (first.price == second.price)
                return first.globalNumber < second.globalNumber;
            return first.price < second.price;
        }
    };
    std::unordered_map<size_t, std::set<Order, std::function<bool(const Order&, const Order&)>>::const_iterator> m_del;
};


#endif //ORDER_BOOK_ORDERBOOK_H
