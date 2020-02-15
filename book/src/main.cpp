#include <iostream>
#include <fstream>

#include "../include/OrderBook.h"

int main()
{
    OrderBook book;
    std::ifstream file("/home/dmitrii/Desktop/exh.txt");
    while(auto val = get_next_order(file))
    {
        book.addOrder(*val);
    }
    std::cout << "\n\n\n";
    book.print();
}
