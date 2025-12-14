cmake_minimum_required(VERSION 3.16)
project(BankingSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PostgreSQL QUIET)

# PostgreSQL support
option(USE_POSTGRESQL "Enable PostgreSQL database integration" ON)

if(USE_POSTGRESQL)
    if(NOT PostgreSQL_FOUND)
        message(FATAL_ERROR "PostgreSQL not found. Install libpq-dev or disable with -DUSE_POSTGRESQL=OFF")
    endif()
    add_definitions(-DUSE_POSTGRESQL)
endif()

# Include directories
include_directories(include)
include_directories(include/network)
include_directories(include/concurrent)
include_directories(include/ai)
include_directories(include/observability)

# Source files
set(NETWORK_SOURCES
    network/tcp_server.cpp
    network/tcp_client.cpp
    network/protocol.cpp
)

set(CONCURRENT_SOURCES
    concurrent/lockfree_queue.cpp
    concurrent/transaction_processor.cpp
)

set(AI_SOURCES
    ai/fraud_detection_agent.cpp
)

set(OBSERVABILITY_SOURCES
    observability/metrics.cpp
    observability/logger.cpp
)

set(DATABASE_SOURCES
    database/postgres_connection.cpp
    database/banking_persistence.cpp
)

set(BANKING_SOURCES
    banking_core_impl.cpp
    banking_system_thread_safe.cpp
    banking_system_persistent.cpp
    banking_server.cpp
)

# Create library targets
add_library(network ${NETWORK_SOURCES})
target_link_libraries(network Threads::Threads)

add_library(concurrent ${CONCURRENT_SOURCES})
target_link_libraries(concurrent Threads::Threads)

add_library(ai ${AI_SOURCES})
target_link_libraries(ai Threads::Threads)

add_library(observability ${OBSERVABILITY_SOURCES})
target_link_libraries(observability Threads::Threads)

add_library(database ${DATABASE_SOURCES})
target_link_libraries(database Threads::Threads)
if(USE_POSTGRESQL)
    target_link_libraries(database PostgreSQL::PostgreSQL)
endif()

add_library(banking ${BANKING_SOURCES})
target_link_libraries(banking network concurrent ai observability Threads::Threads)

if(USE_POSTGRESQL)
    target_link_libraries(banking PostgreSQL::PostgreSQL)
endif()

# Main server executable
add_executable(banking_server main_server.cpp)
target_link_libraries(banking_server banking)

# Client executable
add_executable(banking_client banking_client.cpp)
target_link_libraries(banking_client network)

# Test executable (requires Google Test)
find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(banking_tests tests/test_banking_system.cpp)
    target_link_libraries(banking_tests banking GTest::gtest_main)
    enable_testing()
    add_test(NAME BankingSystemTests COMMAND banking_tests)
else()
    message(WARNING "Google Test not found. Tests will not be built.")
endif()

# Installation
install(TARGETS banking_server banking_client
        RUNTIME DESTINATION bin)

# Custom targets
add_custom_target(run_server
    COMMAND banking_server
    DEPENDS banking_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running banking server"
)

add_custom_target(run_client
    COMMAND banking_client
    DEPENDS banking_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running banking client"
)

# Build options
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_DOCS "Build documentation" OFF)

if(BUILD_DOCS)
    find_package(Doxygen)
    if(Doxygen_FOUND)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
        )
    endif()
endif()

# Print configuration summary
message(STATUS "=== Banking System Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests enabled: ${BUILD_TESTS}")
message(STATUS "Documentation enabled: ${BUILD_DOCS}")
message(STATUS "====================================")
