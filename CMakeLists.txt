cmake_minimum_required(VERSION 3.10)
project(ChronosDB C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include_directories(include)

add_library(chronos_static STATIC src/chronos.c)
target_link_libraries(chronos_static PRIVATE OpenSSL::Crypto Threads::Threads)

add_executable(chronos_main src/main.c)
target_link_libraries(chronos_main PRIVATE chronos_static OpenSSL::Crypto Threads::Threads)

add_executable(test_concurrency tests/test_concurrency.c)
target_link_libraries(test_concurrency PRIVATE chronos_static OpenSSL::Crypto Threads::Threads)

add_executable(test_benchmark tests/test_benchmark.c)
target_link_libraries(test_benchmark PRIVATE chronos_static OpenSSL::Crypto Threads::Threads)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runner.c")
    add_executable(test_runner tests/test_runner.c)
    target_link_libraries(test_runner PRIVATE chronos_static OpenSSL::Crypto Threads::Threads)
endif()

include(GNUInstallDirs)

install(TARGETS chronos_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/chronos.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS chronos_main test_concurrency test_benchmark
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
