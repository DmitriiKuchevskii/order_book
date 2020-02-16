cmake_minimum_required(VERSION 3.13)
project(order_book)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "'" ${CMAKE_SYSTEM_NAME} "' operation system is not supported.
     Please use 'Linux' instead to build this application")
endif()

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
link_libraries(Threads::Threads uuid)

add_compile_options(-Wall -pedantic -Werror -mavx -march=native -fconcepts)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0 -g3)
else()
    add_compile_options(-Ofast -flto)
    add_compile_definitions(NDEBUG)
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-g3)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-g0)
    else()
        message(FATAL_ERROR "'" ${CMAKE_BUILD_TYPE} "' build type is not supported.")
    endif()
endif()

add_subdirectory(book)
add_subdirectory(test)