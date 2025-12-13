cmake_minimum_required(VERSION 3.10)
project(yourDB)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(db
    src/main.cpp
    src/db.cpp
    src/wal.cpp
)
