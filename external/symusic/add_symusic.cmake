FetchContent_Declare(
    symusic
    GIT_REPOSITORY https://github.com/Yikai-Liao/symusic
    GIT_TAG 003e4f798d107833637effef8c730cdd05967467
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
)
FetchContent_MakeAvailable(symusic)
