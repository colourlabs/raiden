cmake_minimum_required(VERSION 3.14)
project(raiden)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CPM.cmake)

add_subdirectory(src/Engine)
add_subdirectory(src/Runtime)
add_subdirectory(src/Shaders)

add_subdirectory(games/ExampleGame)
