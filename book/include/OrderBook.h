//
// Created by dmitrii on 2/15/20.
//

#ifndef ORDER_BOOK_ORDERBOOK_H
#define ORDER_BOOK_ORDERBOOK_H

#include <regex>
#include <atomic>
#include <sstream>
#include <string>
#include <sstream>
#include <set>
#include <unordered_set>
#include <uuid/uuid.h>

constexpr char BuySide = 'B';
constexpr char SellSide = 'S';
constexpr char PlaceAction = 'A';
constexpr char RemoveAction = 'X';

using namespace std::chrono;

struct Order
{
    Order(char side, size_t price, size_t qty)
        : Order(side, price, qty,
             []() -> size_t
             {
                 uuid_t uuid;
                 uuid_generate(uuid);
                 return std::hash<std::string_view>()(std::string_view((char*)uuid, std::size(uuid)));
             }()
         )
    {}

    Order(char side, size_t price, size_t qty, size_t id)
            : side(side), price(price), qty(qty), id(id)
    {}

    Order(const Order& copy) : Order(copy.side, copy.price, copy.qty)
    {}

    Order(Order&& move) noexcept : Order(move.side, move.price, move.qty, move.id)
    {
        placeTime = move.placeTime;
    }

    bool operator == (const Order& other) const
    {
        return id == other.id;
    }

    char side;
    size_t price;
    mutable size_t qty;
    size_t id;
    size_t placeTime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
};

struct Trade
{
    size_t initOrderId;
    size_t matchOrderId;
    size_t executedQty;
    size_t executedPrice;
};

std::ostream& operator << (std::ostream& os, const Order& order)
{
    return os << "Order=[" << "ID: " << order.id << "; "
                    << "Side: " << (order.side == BuySide ? "Buy" : "Sell") << "; "
                    << "Price: " << order.price << "; "
                    << "Quantity: " << order.qty << "]";

}

inline std::pair<char, Order> parse_order(const std::string& line)
{
    std::vector<std::string> vals =
    {
        std::sregex_token_iterator{line.begin(), line.end(), (const std::regex&)std::regex(","), -1}, {}
    };

    return std::make_pair(vals[0][0],
    Order{
          vals[2][0]
        , std::stoull(vals[4])
        , std::stoull(vals[3])
        , std::stoull(vals[1])
    });
}

inline std::optional<std::pair<char, Order>> get_next_order(std::istream& stream)
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
        return first.placeTime < second.placeTime;

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
    using NewTradeListinerHandler = std::function<void(const Trade&)>;
    using ComparatorType = std::function<bool(const Order&, const Order&)>;

public:
    template <typename OrderType>
    void placeOrder(OrderType&& order)
    {
        std::cout << "Request to add a new order " << order << "\n";

        order.side == BuySide ? placeOrder<BuySide>(std::forward<Order>(order)) :
                                placeOrder<SellSide>(std::forward<Order>(order));

    }

    template <typename OrderType>
    void removeOrder(OrderType&& order)
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
        std::cout << "=================\nASK";
        print<SellSide>();
        std::cout << "\n------------";
        print<BuySide>();
        std::cout << "\nBID\n=================\n";
    }

    void addNewTradeListener(const NewTradeListinerHandler& newTradeListener)
    {
        m_newTradeObserver = newTradeListener;
    }

    const auto& getBids() const { return m_bids; }

    const auto& getAsks() const { return m_asks; }

private:
    template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void print() const
    {
        size_t prevPrice = -1;
        auto printVal = [&prevPrice](const Order& order)
        {
            if (prevPrice != order.price)
                std::cout << "\nPrice: " << order.price << "; Quantity: ";
            std::cout  << order.qty << "(id: " << order.id << ") ";
            prevPrice = order.price;
        };
        if constexpr (Side == BuySide)
        {
            for (const auto& order: m_bids)
                printVal(order);
        }
        else
        {
            for (auto it = m_asks.rbegin(); it != m_asks.rend(); ++it)
                printVal(*it);
        }
    }

    template <char Side, typename OrderType, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void placeOrder(OrderType&& order)
    {
        auto& initiatorBook = [this]() -> auto& { return Side == BuySide ? m_bids : m_asks; } ();
        auto insertResult = initiatorBook.emplace(std::forward<Order>(order));
        m_del.emplace(insertResult.first->id, insertResult.first);
        if (isNewTrade())
            doTrades<Side>(initiatorBook);
    }

    bool isNewTrade()
    {
        return !m_bids.empty() && !m_asks.empty() &&
                m_bids.begin()->price >= m_asks.begin()->price;
    }

    template <char Side, typename U = std::enable_if<Side == BuySide || Side == SellSide>>
    void doTrades(auto& initiatorOrderBook)
    {
        constexpr const char* InitSideStr = Side == BuySide ? "Buy" : "Sell";
        constexpr const char* CounterSideStr = Side != BuySide ? "Buy" : "Sell";

        auto& counterOrderBook = Side == BuySide ? m_asks : m_bids;
        auto initiatorOrderIt = initiatorOrderBook.begin();
        for (auto counterOrderIt = counterOrderBook.begin(); counterOrderIt != counterOrderBook.end(); )
        {
            if constexpr (Side == BuySide)
            {
                if (initiatorOrderIt->price < counterOrderBook.begin()->price) return;
            }
            else
            {
                if (initiatorOrderIt->price > counterOrderBook.begin()->price) return;
            }

            std::cout << "Order " << initiatorOrderIt->id << "(" << InitSideStr
                      << ") has initialized a trade with order "
                      << counterOrderIt->id << "(" << CounterSideStr << ")" << " for "
                      << std::min(initiatorOrderIt->qty, counterOrderIt->qty)
                      << " shares at price " << counterOrderIt->price << " USD. ";

            if (initiatorOrderIt->qty > counterOrderIt->qty)
            {
                size_t counterOrderId = counterOrderIt->id;
                size_t counterOrderPrice = counterOrderIt->price;
                size_t counterOrderQty = counterOrderIt->qty;
                initiatorOrderIt->qty -= counterOrderQty;

                m_del.erase(counterOrderId);
                counterOrderIt = counterOrderBook.erase(counterOrderIt);

                m_newTradeObserver({initiatorOrderIt->id, counterOrderId, counterOrderQty, counterOrderPrice});

                std::cout << "Order " << initiatorOrderIt->id << " has been partially traded (Quantity left: "
                          << initiatorOrderIt->qty << "). Order " << counterOrderId << " has been fully traded.\n";
                continue;
            }
            else if (initiatorOrderIt->qty == counterOrderIt->qty)
            {
                size_t initiatorOrderId = initiatorOrderIt->id;
                size_t initiatorOrderQty = initiatorOrderIt->qty;
                size_t counterOrderId = counterOrderIt->id;
                size_t counterOrderPrice = counterOrderIt->price;

                m_del.erase(initiatorOrderId);
                m_del.erase(counterOrderId);
                initiatorOrderBook.erase(initiatorOrderIt);
                counterOrderBook.erase(counterOrderIt);

                m_newTradeObserver({initiatorOrderId, counterOrderId, initiatorOrderQty, counterOrderPrice});

                std::cout << "Both orders have been fully traded.\n";
            }
            else // initiatorOrderIt->qty < counterOrderIt->qty
            {
                counterOrderIt->qty -= initiatorOrderIt->qty;
                size_t initiatorOrderId = initiatorOrderIt->id;
                size_t initiatorOrderQty = initiatorOrderIt->qty;

                m_del.erase(initiatorOrderId);
                initiatorOrderBook.erase(initiatorOrderIt);

                m_newTradeObserver({initiatorOrderId, counterOrderIt->id, initiatorOrderQty, counterOrderIt->price});

                std::cout << "Order " << initiatorOrderId << " has been fully traded. Order "
                          << counterOrderIt->id << " has been partially traded (Quantity left: "
                          << counterOrderIt->qty << ")\n";
            }
            return;
        }
    }

private:
    std::set<Order, ComparatorType> m_bids{OrderBookComparator<BuySide>};
    std::set<Order, ComparatorType> m_asks{OrderBookComparator<SellSide>};
    std::unordered_map<size_t, decltype(m_bids)::const_iterator> m_del;
    NewTradeListinerHandler m_newTradeObserver = [](const Trade&){};
};


#endif //ORDER_BOOK_ORDERBOOK_H
