cmake_minimum_required(VERSION 3.26)
project(ClientServer3)

set(CMAKE_CXX_STANDARD 14)

include_directories(client1, client2)
include_directories(headers)

add_executable(client1 client.cpp)

add_executable(client2 client.cpp)

add_executable(server server.cpp
        headers/file.cpp
        headers/room.cpp)
