cmake_minimum_required(VERSION 3.19...3.30)
project(tokenizers_cpp C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(TOKENIZERS_CPP_MSVC_RUNTIME_LIBRARY "" CACHE STRING "MSVC runtime library setting (One of MT or MD)")

# update to contain more rust flags
set(TOKENIZERS_CPP_RUST_FLAGS "")
set(TOKENIZERS_CPP_CARGO_TARGET "")

if(MSVC)
  if(TOKENIZERS_CPP_MSVC_RUNTIME_LIBRARY STREQUAL "MT")
    list(APPEND TOKENIZERS_CPP_RUST_FLAGS -Ctarget-feature=+crt-static)
  elseif(TOKENIZERS_CPP_MSVC_RUNTIME_LIBRARY STREQUAL "MD")
    list(APPEND TOKENIZERS_CPP_RUST_FLAGS -Ctarget-feature=-crt-static)
  elseif(TOKENIZERS_CPP_MSVC_RUNTIME_LIBRARY)
    message(WARNING "Invalid value for TOKENIZERS_CPP_MSVC_RUNTIME_LIBRARY. Ignoring.")
  endif()
endif()

# extra link libraries
set(TOKENIZERS_CPP_LINK_LIBS "")
set(TOKENIZERS_C_LINK_LIBS "")
set(CARGO_EXTRA_ENVS "")
message(STATUS "system-name" ${CMAKE_SYSTEM_NAME})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND TOKENIZERS_C_LINK_LIBS ${CMAKE_DL_LIBS})
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(TOKENIZERS_CPP_CARGO_TARGET aarch64-unknown-linux-gnu)
  endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(TOKENIZERS_CPP_CARGO_TARGET wasm32-unknown-emscripten)
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  if (CMAKE_OSX_SYSROOT MATCHES ".*iPhoneSimulator\\.platform.*")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
      set(TOKENIZERS_CPP_CARGO_TARGET x86_64-apple-ios)
    else ()
      set(TOKENIZERS_CPP_CARGO_TARGET aarch64-apple-ios-sim)
    endif ()
  else ()
    set(TOKENIZERS_CPP_CARGO_TARGET aarch64-apple-ios)
  endif ()
  # add extra dependency needed for rust tokenizer in iOS
  find_library(FOUNDATION_LIB Foundation)
  find_library(SECURITY_LIB Security)
  list(APPEND TOKENIZERS_C_LINK_LIBS ${FOUNDATION_LIB} ${SECURITY_LIB})
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # Check for universal (fat) binary build
  set(TOKENIZERS_CPP_BUILD_UNIVERSAL OFF)
  list(LENGTH CMAKE_OSX_ARCHITECTURES _osx_arch_count)
  if (_osx_arch_count GREATER 1)
    set(TOKENIZERS_CPP_BUILD_UNIVERSAL ON)
    set(TOKENIZERS_CPP_CARGO_TARGET_ARM64 aarch64-apple-darwin)
    set(TOKENIZERS_CPP_CARGO_TARGET_X86_64 x86_64-apple-darwin)
  else()
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
      set(TOKENIZERS_CPP_CARGO_TARGET aarch64-apple-darwin)
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
      set(TOKENIZERS_CPP_CARGO_TARGET x86_64-apple-darwin)
    endif()
  endif()
  if (CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CARGO_EXTRA_ENVS
      MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    )
  endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  if (ANDROID_ABI STREQUAL "arm64-v8a")
    set(TOKENIZERS_CPP_CARGO_TARGET aarch64-linux-android)
  elseif (ANDROID_ABI STREQUAL "armeabi-v7a")
    set(TOKENIZERS_CPP_CARGO_TARGET armv7-linux-androideabi)
  elseif (ANDROID_ABI STREQUAL "x86_64")
    set(TOKENIZERS_CPP_CARGO_TARGET x86_64-linux-android)
  elseif (ANDROID_ABI STREQUAL "x86")
    set(TOKENIZERS_CPP_CARGO_TARGET i686-linux-android)
  endif()
  set(CARGO_EXTRA_ENVS
    AR_${TOKENIZERS_CPP_CARGO_TARGET}=${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar
    CC_${TOKENIZERS_CPP_CARGO_TARGET}=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOKENIZERS_CPP_CARGO_TARGET}${ANDROID_NATIVE_API_LEVEL}-clang
    CXX_${TOKENIZERS_CPP_CARGO_TARGET}=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOKENIZERS_CPP_CARGO_TARGET}${ANDROID_NATIVE_API_LEVEL}-clang++
  )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(TOKENIZERS_CPP_CARGO_TARGET aarch64-pc-windows-msvc)
  else()
    set(TOKENIZERS_CPP_CARGO_TARGET x86_64-pc-windows-msvc)
  endif()
endif()


if(WIN32)
  list(APPEND TOKENIZERS_C_LINK_LIBS
    ntdll wsock32 ws2_32 Bcrypt
    iphlpapi userenv psapi
  )
endif()

set(TOKENIZERS_CPP_CARGO_FLAGS "")
set(TOKENIZERS_CPP_CARGO_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(TOKENIZERS_CPP_CARGO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if (NOT TOKENIZERS_CPP_CARGO_TARGET STREQUAL "")
  list(APPEND TOKENIZERS_CPP_CARGO_FLAGS --target ${TOKENIZERS_CPP_CARGO_TARGET})
  set(TOKENIZERS_CPP_CARGO_BINARY_DIR
        "${TOKENIZERS_CPP_CARGO_BINARY_DIR}/${TOKENIZERS_CPP_CARGO_TARGET}")
endif()

set(TOKENIZERS_CPP_CARGO_RELEASE_FLAG "")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(TOKENIZERS_CPP_CARGO_BUILD_TYPE "debug")
else ()
  set(TOKENIZERS_CPP_CARGO_BUILD_TYPE "release")
  set(TOKENIZERS_CPP_CARGO_RELEASE_FLAG "--release")
  list(APPEND TOKENIZERS_CPP_CARGO_FLAGS --release)
endif ()

set(TOKENIZERS_CPP_CARGO_BINARY_DIR "${TOKENIZERS_CPP_CARGO_BINARY_DIR}/${TOKENIZERS_CPP_CARGO_BUILD_TYPE}")

get_filename_component(TOKENIZERS_CPP_ROOT ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)
set(TOKENIZERS_CPP_CARGO_SOURCE_PATH ${TOKENIZERS_CPP_ROOT}/rust)

option(MSGPACK_USE_BOOST "Use Boost libraried" OFF)
add_subdirectory(msgpack)

option(MLC_ENABLE_SENTENCEPIECE_TOKENIZER "Enable SentencePiece tokenizer" ON)

if(MSVC)
  set(TOKENIZERS_RUST_LIB "${TOKENIZERS_CPP_CARGO_BINARY_DIR}/tokenizers_c.lib")
else()
  set(TOKENIZERS_RUST_LIB "${TOKENIZERS_CPP_CARGO_BINARY_DIR}/libtokenizers_c.a")
endif()
set(TOKENIZERS_CPP_INCLUDE ${TOKENIZERS_CPP_ROOT}/include)

# Find cargo executable
if(CMAKE_HOST_WIN32)
  set(USER_HOME "$ENV{USERPROFILE}")
else()
  set(USER_HOME "$ENV{HOME}")
endif()
if(NOT DEFINED CARGO_HOME)
  if("$ENV{CARGO_HOME}" STREQUAL "")
    set(CARGO_HOME "${USER_HOME}/.cargo")
  else()
    set(CARGO_HOME "$ENV{CARGO_HOME}")
  endif()
endif()

find_program(CARGO_EXECUTABLE cargo
 HINTS "${CARGO_HOME}"
 PATH_SUFFIXES "bin")
mark_as_advanced(CARGO_EXECUTABLE)

if (TOKENIZERS_CPP_BUILD_UNIVERSAL)
  # Universal (fat) binary build: build Rust library for each architecture
  # separately, then merge with lipo.
  set(TOKENIZERS_RUST_LIB_ARM64
    "${TOKENIZERS_CPP_CARGO_TARGET_DIR}/${TOKENIZERS_CPP_CARGO_TARGET_ARM64}/${TOKENIZERS_CPP_CARGO_BUILD_TYPE}/libtokenizers_c.a")
  set(TOKENIZERS_RUST_LIB_X86_64
    "${TOKENIZERS_CPP_CARGO_TARGET_DIR}/${TOKENIZERS_CPP_CARGO_TARGET_X86_64}/${TOKENIZERS_CPP_CARGO_BUILD_TYPE}/libtokenizers_c.a")
  set(TOKENIZERS_RUST_LIB "${CMAKE_CURRENT_BINARY_DIR}/libtokenizers_c.a")

  add_custom_command(
    OUTPUT ${TOKENIZERS_RUST_LIB}
    # Build for arm64
    COMMAND ${CMAKE_COMMAND} -E env
      CARGO_TARGET_DIR=${TOKENIZERS_CPP_CARGO_TARGET_DIR}
      ${CARGO_EXTRA_ENVS}
      RUSTFLAGS="${TOKENIZERS_CPP_RUST_FLAGS}"
      ${CARGO_EXECUTABLE} build --target ${TOKENIZERS_CPP_CARGO_TARGET_ARM64}
      ${TOKENIZERS_CPP_CARGO_RELEASE_FLAG}
    # Build for x86_64
    COMMAND ${CMAKE_COMMAND} -E env
      CARGO_TARGET_DIR=${TOKENIZERS_CPP_CARGO_TARGET_DIR}
      ${CARGO_EXTRA_ENVS}
      RUSTFLAGS="${TOKENIZERS_CPP_RUST_FLAGS}"
      ${CARGO_EXECUTABLE} build --target ${TOKENIZERS_CPP_CARGO_TARGET_X86_64}
      ${TOKENIZERS_CPP_CARGO_RELEASE_FLAG}
    # Merge into universal binary
    COMMAND lipo -create
      ${TOKENIZERS_RUST_LIB_ARM64}
      ${TOKENIZERS_RUST_LIB_X86_64}
      -output ${TOKENIZERS_RUST_LIB}
    WORKING_DIRECTORY ${TOKENIZERS_CPP_CARGO_SOURCE_PATH}
    COMMENT "Building universal (arm64 + x86_64) libtokenizers_c.a"
  )
else()
  # Single-architecture build
  # NOTE: need to use cmake -E env to be portable in win
  add_custom_command(
    OUTPUT ${TOKENIZERS_RUST_LIB}
    COMMAND
    ${CMAKE_COMMAND} -E env
    CARGO_TARGET_DIR=${TOKENIZERS_CPP_CARGO_TARGET_DIR}
    ${CARGO_EXTRA_ENVS}
    RUSTFLAGS="${TOKENIZERS_CPP_RUST_FLAGS}"
    ${CARGO_EXECUTABLE} build ${TOKENIZERS_CPP_CARGO_FLAGS}
    WORKING_DIRECTORY ${TOKENIZERS_CPP_CARGO_SOURCE_PATH}
    POST_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy
    ${TOKENIZERS_RUST_LIB} "${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()

# Create a custom target so that the cargo build is guaranteed to run
# before anything links against tokenizers_c. add_custom_command alone
# only runs if a target in the SAME directory depends on its OUTPUT;
# with Xcode and multi-directory builds the dependency can be missed.
add_custom_target(tokenizers_c_build DEPENDS ${TOKENIZERS_RUST_LIB})

set(
  TOKENIZER_CPP_SRCS
  src/sentencepiece_tokenizer.cc
  src/huggingface_tokenizer.cc
  src/rwkv_world_tokenizer.cc
)
add_library(tokenizers_cpp STATIC ${TOKENIZER_CPP_SRCS})
target_include_directories(tokenizers_cpp PRIVATE sentencepiece/src)
target_include_directories(tokenizers_cpp PRIVATE msgpack/include)
target_include_directories(tokenizers_cpp PUBLIC ${TOKENIZERS_CPP_INCLUDE})
if (MLC_ENABLE_SENTENCEPIECE_TOKENIZER STREQUAL "ON")
  target_compile_definitions(tokenizers_cpp PUBLIC MLC_ENABLE_SENTENCEPIECE_TOKENIZER)
endif ()
target_link_libraries(tokenizers_cpp PRIVATE msgpack-cxx)

# sentencepiece config
option(SPM_ENABLE_SHARED "override sentence piece config" OFF)
option(SPM_ENABLE_TCMALLOC "" OFF)
# provide macro if it does not exist in cmake system
# it is OK to skip those since we do not provide these apps in the ios
# instead just link to the sentencepiece directly
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
    set_property (TARGET ${TARGET} PROPERTY
          XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
  endmacro (set_xcode_property)
endif()
add_subdirectory(sentencepiece sentencepiece EXCLUDE_FROM_ALL)

add_library(tokenizers_c INTERFACE)
add_dependencies(tokenizers_c tokenizers_c_build)
target_link_libraries(tokenizers_c INTERFACE ${TOKENIZERS_RUST_LIB} ${TOKENIZERS_C_LINK_LIBS})

target_link_libraries(tokenizers_cpp PRIVATE tokenizers_c sentencepiece-static ${TOKENIZERS_CPP_LINK_LIBS})
target_include_directories(tokenizers_cpp PUBLIC ${TOKENIZERS_CPP_INCLUDE})
