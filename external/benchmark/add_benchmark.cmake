set(BENCHMARK_ENABLE_TESTING off)
set(BENCHMARK_ENABLE_GTEST_TESTS off)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.4
)
FetchContent_MakeAvailable(benchmark)
