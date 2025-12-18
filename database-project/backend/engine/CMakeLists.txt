cmake_minimum_required(VERSION 3.20)

project(db_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------ COMMON ENGINE SOURCES ------------------
set(ENGINE_CORE
    src/database_engine.cpp
    src/storage.cpp
    src/wal.cpp
    src/logger.cpp
    src/transaction_manager.cpp
    src/query_parser.cpp
    
)

# ------------------ MAIN ENGINE (SERVER) ------------------
add_executable(db_engine
    src/main.cpp
    src/server.cpp
    ${ENGINE_CORE}
)

target_include_directories(db_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(db_engine Ws2_32)

# ------------------ TEST EXECUTABLE ------------------
add_executable(db_engine_test
    src/test.cpp
    ${ENGINE_CORE}
)

target_include_directories(db_engine_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ------------------ OPTIONAL: INTERACTIVE CLI ------------------
