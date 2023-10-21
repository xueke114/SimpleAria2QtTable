cmake_minimum_required(VERSION 3.15)

project(SimpleAria2QtDemo LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt6 COMPONENTS Widgets Network)

add_executable(SimpleAria2QtTable SimpleAria2QtTable.cpp)
target_link_libraries(SimpleAria2QtTable Qt6::Widgets aria2)

add_executable(SimpleAria2PRCqtTable SimpleAria2RPCqtTable.cpp)
target_link_libraries(SimpleAria2PRCqtTable Qt6::Widgets Qt6::Network)

# 兼容Qt5
