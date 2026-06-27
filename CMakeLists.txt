cmake_minimum_required(VERSION 3.14)
project(raiden)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CPM.cmake)

add_subdirectory(src/EngineCore)
add_subdirectory(src/EngineRuntime)
add_subdirectory(shaders)