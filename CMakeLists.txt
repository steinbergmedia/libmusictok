cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(libmusictok LANGUAGES CXX C VERSION 1.0.0)

option(LIBMUSICTOK_BUILD_TESTS "Build LibMusicTok tests" OFF)
option(LIBMUSICTOK_BUILD_BENCHMARKS "Build LibMusicTok benchmarks" OFF)

include(external/Dependencies.cmake)

add_compile_definitions(PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_library(LibMusicTok)
target_include_directories(LibMusicTok PUBLIC include)

set(SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizer_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/attribute_control.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/utility_functions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/token_id_converter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/event.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/mumidi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/cpword.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/structured.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/music_tokenizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/mmm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/remi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/pertok.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/octuple.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/midilike.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tokenizations/tsd.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/tok_sequence.cpp"
)

set(HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/event.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tok_sequence.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/constants.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/token_id_converter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/utility_functions.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/structured.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/octuple.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/tsd.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/remi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/pertok.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/mmm.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/cpword.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/midilike.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/music_tokenizer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizations/mumidi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/attribute_control.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libmusictok/tokenizer_config.h"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "include" FILES ${HEADER_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/source" PREFIX "source" FILES ${SOURCE_FILES})

target_sources(LibMusicTok PRIVATE ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(LibMusicTok PUBLIC symusic tokenizers_cpp)
target_include_directories(LibMusicTok PUBLIC ${symusic_SOURCE_DIR}/include ${tokenizers_cpp_SOURCE_DIR}/include)

target_link_libraries(LibMusicTok PRIVATE nlohmann_json::nlohmann_json)

if(LIBMUSICTOK_BUILD_TESTS)
    add_executable(LibMusicTokTest)
    target_link_libraries(LibMusicTokTest LibMusicTok GTest::gtest_main GTest::gtest symusic tokenizers_cpp nlohmann_json::nlohmann_json)
    set(TEST_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/test/tok_sequence_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/token_id_converter_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/tokenizer_config_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/utility_functions_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/tokenizers_cpp_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/test_tokenize.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/test_preprocess.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/testing_utilities.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/event_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/attribute_control_test.cpp"
    )
    target_sources(LibMusicTokTest PRIVATE ${TEST_FILES})

    add_custom_command(
        TARGET LibMusicTokTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:LibMusicTokTest>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/MIDIs_corrupted"
            "$<TARGET_FILE_DIR:LibMusicTokTest>/resources/scores/MIDIs_corrupted"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/Multitrack_MIDIs"
            "$<TARGET_FILE_DIR:LibMusicTokTest>/resources/scores/Multitrack_MIDIs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/One_track_MIDIs"
            "$<TARGET_FILE_DIR:LibMusicTokTest>/resources/scores/One_track_MIDIs"
    )

    target_compile_definitions(LibMusicTokTest PRIVATE RUNTIME_OUTPUT_DIRECTORY="$<TARGET_FILE_DIR:LibMusicTokTest>")
    target_compile_definitions(LibMusicTokTest PRIVATE RESOURCES_DIRECTORY="$<TARGET_FILE_DIR:LibMusicTokTest>/resources")
endif()

if(LIBMUSICTOK_BUILD_BENCHMARKS)
    add_executable(LibMusicTokBenchmark)
    target_link_libraries(LibMusicTokBenchmark LibMusicTok benchmark symusic tokenizers_cpp nlohmann_json::nlohmann_json)
    file(GLOB_RECURSE BENCHMARK_FILES "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/*.cpp")
    set(BENCHMARK_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/midilike_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/octuple_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/remi_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/tsd_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/mumidi_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/mmm_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/structured_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pertok_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/cpword_benchmark.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/music_tokenizer_benchmark.cpp"
    )
    target_sources(LibMusicTokBenchmark PRIVATE ${BENCHMARK_FILES})

    add_custom_command(
        TARGET LibMusicTokBenchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:LibMusicTokBenchmark>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/MIDIs_corrupted"
            "$<TARGET_FILE_DIR:LibMusicTokBenchmark>/resources/scores/MIDIs_corrupted"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/Multitrack_MIDIs"
            "$<TARGET_FILE_DIR:LibMusicTokBenchmark>/resources/scores/Multitrack_MIDIs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${symusic_SOURCE_DIR}/tests/testcases/One_track_MIDIs"
            "$<TARGET_FILE_DIR:LibMusicTokBenchmark>/resources/scores/One_track_MIDIs"
    )

    target_compile_definitions(LibMusicTokBenchmark PRIVATE RUNTIME_OUTPUT_DIRECTORY="$<TARGET_FILE_DIR:LibMusicTokBenchmark>")
    target_compile_definitions(LibMusicTokBenchmark PRIVATE RESOURCES_DIRECTORY="$<TARGET_FILE_DIR:LibMusicTokBenchmark>/resources")
endif()
