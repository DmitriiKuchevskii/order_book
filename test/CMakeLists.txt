project(order_book_test)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../book/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../googletest/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../googletest/googlemock/include)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../googletest/build/lib/)

add_executable(order_book_test order_book_test.cpp)

target_link_libraries(order_book_test
        gtest_main
        gtest
        gmock_main
        gmock
)

