cmake_minimum_required(VERSION 3.20)

project(db_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(db_engine
    src/main.cpp
    src/server.cpp
    src/database_engine.cpp
    src/storage.cpp
    src/wal.cpp
    src/logger.cpp
    src/transaction_manager.cpp
)

# ✅ TELL COMPILER WHERE HEADERS ARE
target_include_directories(db_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ✅ WINDOWS SOCKETS
target_link_libraries(db_engine Ws2_32)
