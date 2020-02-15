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
    mutable size_t qty;
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


template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
bool OrderBookComparator(const Order& first, const Order& second)
{
    if (first.price == second.price)
        return first.globalNumber < second.globalNumber;

    if constexpr (Side == BuySide)
    {
        return first.price > second.price;
    }
    else
    {
        return first.price < second.price;
    }
}

class OrderBook
{
public:
    void placeOrder(const Order& order)
    {
        std::cout << "Request to add a new order " << order << "\n";

        order.side == BuySide ? placeOrder<BuySide>(order) : placeOrder<SellSide>(order);
    }

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

    void print() const
    {
        print<SellSide>();
        print<BuySide>();
    }

private:
    template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void print() const
    {
        std::cout << (Side == SellSide ? "=================\nASK" : "");
        size_t prevPrice = -1;
        const auto& book = Side == BuySide ? m_asks : m_bids;
        for (const auto& order: book)
        {
            if (prevPrice != order.price)
                std::cout << "\n" << order.price << ": ";

            std::cout  << order.qty << " ";
            prevPrice = order.price;
        }
        std::cout << (Side == SellSide ? "\n------------" : "\nBID\n=================\n");
    }

    template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void placeOrder(const Order& order)
    {
        auto& initiatorBook = [this]() -> auto& { return Side == BuySide ? m_bids : m_asks; } ();
        auto insertResult = initiatorBook.emplace(order);
        m_del.emplace(order.id, insertResult.first);
        if (isNewTrade())
            doTrades<Side>(initiatorBook);
    }

    bool isNewTrade()
    {
        return !m_bids.empty() && !m_asks.empty() &&
                m_bids.begin()->price >= m_asks.begin()->price;
    }

    template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void doTrades(auto& initiatorBook)
    {
        constexpr const char* InitSideStr = Side == BuySide ? "Buy" : "Sell";
        constexpr const char* CounterSideStr = Side != BuySide ? "Buy" : "Sell";

        auto& counter = Side == BuySide ? m_asks : m_bids;
        auto initiator = initiatorBook.begin();
        for (auto it = counter.begin(); it != counter.end(); )
        {
            if constexpr (Side == BuySide)
            {
                if (initiator->price < counter.begin()->price) return;
            }
            else
            {
                if (initiator->price > counter.begin()->price) return;
            }

            std::cout << "Order "<< initiator->id << "(" << InitSideStr <<  ") has initialized a trade with order "
                      << it->id << "(" << CounterSideStr << ")" << " for " << std::min(initiator->qty, it->qty)
                      << " shares at price " << it->price << " USD. ";

            if (initiator->qty > it->qty)
            {
                initiator->qty -= it->qty;
                m_del.erase(it->id);
                size_t counterId = it->id;
                it = counter.erase(it);
                std::cout << "Order " << initiator->id << " has been partially traded (Quantity left: "
                          << initiator->qty << "). Order " << counterId << " has been fully traded.\n";
                continue;
            }
            else if (initiator->qty == it->qty)
            {
                m_del.erase(initiator->id);
                m_del.erase(it->id);
                initiatorBook.erase(initiator);
                counter.erase(it);
                std::cout << "Both orders have been fully traded.\n";
                return;
            }
            else // initiator->qty < it->qty
            {
                it->qty -= initiator->qty;
                m_del.erase(initiator->id);
                size_t initId = initiator->id;
                initiatorBook.erase(initiator);
                std::cout << "Order " << initId << " has been fully traded. Order "
                          << it->id << " has been partially traded (Quantity left: " << it->qty << ")\n";
                return;
            }
        }
    }

private:
    using ComparatorType = std::function<bool(const Order&, const Order&)>;
    std::set<Order, ComparatorType> m_bids{OrderBookComparator<BuySide>};
    std::set<Order, ComparatorType> m_asks{OrderBookComparator<SellSide>};
    std::unordered_map<size_t, decltype(m_bids)::const_iterator> m_del;
};


#endif //ORDER_BOOK_ORDERBOOK_H
