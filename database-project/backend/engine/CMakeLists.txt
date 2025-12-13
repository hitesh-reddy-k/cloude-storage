cmake_minimum_required(VERSION 3.15)
project(database_engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(db_engine
    src/main.cpp
    src/database_engine.cpp
    src/storage.cpp
    src/wal.cpp
    src/logger.cpp
)

