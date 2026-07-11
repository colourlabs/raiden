raiden_benchmark(ecs_benchmark
  SOURCES
    ecs_view_benchmark.cpp
  LINK_LIBRARIES
    EngineCore
)

raiden_benchmark(ecs_mutation_benchmark
  SOURCES
    ecs_mutation_benchmark.cpp
  LINK_LIBRARIES
    EngineCore
)

raiden_benchmark(transform_hierarchy_benchmark
  SOURCES
    transform_hierarchy_benchmark.cpp
  LINK_LIBRARIES
    EngineCore
)

raiden_benchmark(ecs_system_benchmark
  SOURCES
    ecs_system_benchmark.cpp
  LINK_LIBRARIES
    EngineCore
)
