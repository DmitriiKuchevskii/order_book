# order_book
Simple order book with trades

To build:
  1. Run setup.sh to setup envirioment
  2. mkdir build && cd build && cmake --build-64bit -DCMAKE_BUILD_TYPE=Release .. && make -j
  
To run order book
./book/order_book <input_file_name>

To run tests:
./test/order_book_test
