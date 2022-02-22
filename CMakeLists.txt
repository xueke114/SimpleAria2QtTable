cmake_minimum_required(VERSION 3.15)

project(RsTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6Widgets)
add_executable(SimpleAria2QtTable SimpleAria2QtTable.cpp)

set_target_properties(SimpleAria2QtTable PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

target_compile_options(SimpleAria2QtTable PRIVATE -Wall -Wextra -Werror)

target_link_libraries(SimpleAria2QtTable Qt6::Widgets aria2)

# 兼容Qt5
