cmake_minimum_required(VERSION 3.24)
include(FetchContent)

# GOOGLE BENCHMARK #
include(external/benchmark/add_benchmark.cmake)

# GOOGLE TEST #
include(external/gtest/add_gtest.cmake)

# TOKENIZERS #
include(external/tokenizers_cpp/add_tokenizers_cpp.cmake)

# SYMUSIC #
include(external/symusic/add_symusic.cmake)

# JSON #
include(external/json/add_json.cmake)
