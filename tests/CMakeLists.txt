CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  GIT_TAG v1.15.2
  OPTIONS
    "INSTALL_GTEST OFF"
)

CPMAddPackage(
  NAME googlebenchmark
  GITHUB_REPOSITORY google/benchmark
  GIT_TAG v1.9.1
  OPTIONS
    "BENCHMARK_ENABLE_TESTING OFF"
)

function(raiden_test NAME)
  cmake_parse_arguments(TEST "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})

  add_executable(${NAME} ${TEST_SOURCES})
  target_link_libraries(${NAME} PRIVATE
    gtest_main
    ${TEST_LINK_LIBRARIES}
  )
  target_compile_options(${NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-subobject-linkage>
  )
  add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

function(raiden_benchmark NAME)
  cmake_parse_arguments(BENCH "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})

  add_executable(${NAME} ${BENCH_SOURCES})
  target_link_libraries(${NAME} PRIVATE
    benchmark::benchmark
    ${BENCH_LINK_LIBRARIES}
  )
endfunction()

add_subdirectory(assets)
add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(ecs)
add_subdirectory(jobs)
add_subdirectory(scene)
add_subdirectory(vfs)
add_subdirectory(input)
add_subdirectory(benchmarks)
