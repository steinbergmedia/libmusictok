FetchContent_Declare(
    symusic
    GIT_REPOSITORY https://github.com/Yikai-Liao/symusic
    GIT_TAG 937a74e336158673e3a1de995c3bbbb0cc3608ad
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
)
FetchContent_MakeAvailable(symusic)
