cmake_minimum_required(VERSION 3.20)
project(MyLibMusicTokApp LANGUAGES CXX)

# Enable FetchContent to get dependencies
include(FetchContent)

# add_subdirectory(external)
include(external/Dependencies.cmake)

FetchContent_Declare(
    LibMusicTok
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
FetchContent_MakeAvailable(LibMusicTok)

# Add your executable
add_executable(MyLibMusicTokApp src/main.cpp)

# Link the fetched library
target_link_libraries(MyLibMusicTokApp PRIVATE LibMusicTok nlohmann_json::nlohmann_json symusic tokenizers_cpp)
