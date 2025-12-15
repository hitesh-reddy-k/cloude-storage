cmake_minimum_required(VERSION 3.20)

project(db_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine source files
set(ENGINE_SOURCES
    src/main.cpp       # only include this in db_engine
    src/server.cpp
    src/database_engine.cpp
    src/storage.cpp
    src/wal.cpp
    src/logger.cpp
    src/transaction_manager.cpp
    src/query_parser.cpp
)

# ------------------ MAIN ENGINE ------------------
add_executable(db_engine
    ${ENGINE_SOURCES}
)

target_include_directories(db_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(db_engine Ws2_32)

# ------------------ TEST EXECUTABLE ------------------
add_executable(db_engine_test
    test/test.cpp          # your test file
    src/database_engine.cpp
    src/storage.cpp
    src/wal.cpp
    src/logger.cpp
    src/transaction_manager.cpp
    src/query_parser.cpp
    # do NOT include main.cpp or server.cpp
)

target_include_directories(db_engine_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(db_engine_test Ws2_32)
