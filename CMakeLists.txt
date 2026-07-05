cmake_minimum_required(VERSION 3.21)
project(raiden)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(cmake/CPM.cmake)

function(raiden_set_compile_options target)
  target_compile_options(${target} PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
      -Wall -Wextra -Wpedantic
      -Wformat=2
      -Wshadow
      -Wcast-align
      -Wnull-dereference
      -Wduplicated-cond
      -Wmisleading-indentation
      -Wvla
    >
    $<$<CXX_COMPILER_ID:MSVC>:
      /W4
    >
  )
endfunction()

add_subdirectory(src/Engine)
add_subdirectory(src/Runtime)
add_subdirectory(src/Shaders)
add_subdirectory(src/RaidenUI)
add_subdirectory(src/RaidenEditor)

add_subdirectory(games/ExampleGame)
add_subdirectory(games/EditorData)
