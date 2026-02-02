cmake_minimum_required(VERSION 3.24)
include(FetchContent)

# TOKENIZERS #
include(external/tokenizers_cpp/add_tokenizers_cpp.cmake)

# SYMUSIC #
include(external/symusic/add_symusic.cmake)

# JSON #
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)
