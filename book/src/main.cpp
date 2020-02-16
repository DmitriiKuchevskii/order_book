#include <iostream>
#include <fstream>
#include <filesystem>

#include "../include/OrderBook.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Please specify input file\n";
        exit(EXIT_FAILURE);
    }
    else if (!std::filesystem::exists(argv[1]))
    {
        std::cerr << "File does not exist\n";
        exit(EXIT_FAILURE);
    }

    OrderBook book;
    std::ifstream file(argv[1]);
    while(auto val = get_next_order(file))
    {
        if (auto[action, order] = *val; action == PlaceAction)
            book.placeOrder(order);
        else
            book.removeOrder(order);
    }
    std::cout << "\n\n\n";
    book.print();
}
