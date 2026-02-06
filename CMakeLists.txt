cmake_minimum_required(VERSION 3.16)
project(NovacoinBlockchain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/NovacoinTargets.cmake)

add_library(novacoin_core
    ${NOVACOIN_CORE_SOURCES}
)

target_include_directories(novacoin_core PUBLIC include)

add_executable(novacoin src/main.cpp)
target_link_libraries(novacoin PRIVATE novacoin_core)


add_executable(novacoin-tx src/apps/novacoin-tx.cpp)
target_link_libraries(novacoin-tx PRIVATE novacoin_core)

add_executable(novacoin-wallet src/apps/novacoin-wallet.cpp)
target_link_libraries(novacoin-wallet PRIVATE novacoin_core)

add_executable(novacoin-cli src/apps/novacoin-cli.cpp)
target_link_libraries(novacoin-cli PRIVATE novacoin_core)

add_executable(novacoind src/apps/novacoind.cpp)
target_link_libraries(novacoind PRIVATE novacoin_core)

add_executable(novacoin-regtest src/apps/novacoin-regtest.cpp)
target_link_libraries(novacoin-regtest PRIVATE novacoin_core)


if (MSVC)
    target_compile_options(novacoin_core PRIVATE /W4)
else()
    target_compile_options(novacoin_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(novacoin_tests tests/blockchain_tests.cpp)
target_link_libraries(novacoin_tests PRIVATE novacoin_core)
add_test(NAME novacoin_tests COMMAND novacoin_tests)
