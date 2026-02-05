cmake_minimum_required(VERSION 3.16)
project(NovacoinBlockchain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(novacoin
    src/main.cpp
    src/block.cpp
    src/blockchain.cpp
    src/transaction.cpp
)

target_include_directories(novacoin PRIVATE include)

if (MSVC)
    target_compile_options(novacoin PRIVATE /W4)
else()
    target_compile_options(novacoin PRIVATE -Wall -Wextra -Wpedantic)
endif()
