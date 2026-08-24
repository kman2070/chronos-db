cmake_minimum_required(VERSION 3.10)
project(chronos_db C)

set(CMAKE_C_STANDARD 11)

# Find OpenSSL library
find_package(OpenSSL REQUIRED)

include_directories(include ${OPENSSL_INCLUDE_DIR})

# Main Executable
add_executable(chronos_main src/main.c src/chronos.c src/election.c src/ares_bft.c)
target_link_libraries(chronos_main PRIVATE OpenSSL::Crypto)

# Existing Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_concurrency.c")
    add_executable(test_concurrency tests/test_concurrency.c src/chronos.c)
    target_link_libraries(test_concurrency PRIVATE OpenSSL::Crypto)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_benchmark.c")
    add_executable(test_benchmark tests/test_benchmark.c src/chronos.c)
    target_link_libraries(test_benchmark PRIVATE OpenSSL::Crypto)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runner.c")
    add_executable(test_runner tests/test_runner.c src/chronos.c)
    target_link_libraries(test_runner PRIVATE OpenSSL::Crypto)
endif()

add_executable(test_bft_stress tests/test_bft_stress.c src/ares_bft.c src/chronos.c)
target_link_libraries(test_bft_stress PRIVATE OpenSSL::Crypto)

add_executable(test_tamper tests/test_tamper.c src/ares_bft.c)
target_link_libraries(test_tamper PRIVATE OpenSSL::Crypto)

add_executable(test_bft_votes tests/test_bft_votes.c src/ares_bft.c)
target_link_libraries(test_bft_votes PRIVATE OpenSSL::Crypto)

add_executable(test_bft_hard tests/test_bft_hard.c src/ares_bft.c)
target_link_libraries(test_bft_hard PRIVATE OpenSSL::Crypto)
