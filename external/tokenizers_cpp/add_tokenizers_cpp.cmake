FetchContent_Declare(
    tokenizers_cpp
    GIT_REPOSITORY https://github.com/mlc-ai/tokenizers-cpp.git
    GIT_TAG 55d53aa38dc8df7d9c8bd9ed50907e82ae83ce66
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
)
FetchContent_MakeAvailable(tokenizers_cpp)
