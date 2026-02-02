# LibMusicTok

[![GitHub CI](https://github.com/steinbergmedia/libmusictok/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/steinbergmedia/libmusictok/actions/workflows/cmake-multi-platform.yml)

LibMusicTok is a C++ port of the python [MIDITok](https://github.com/Natooz/MidiTok?tab=readme-ov-file) library
for faster tokenization of symbolic music in realtime music software applications. This library is maintained by Steinberg Media Technologies GmbH.

## Supported Operating Softwares:
- Ubuntu, using the g++ compiler
- Windows, using the MSVC compiler
- macOS, using the AppleClang compiler

### Requirements:
- C++ 20
- CMake 3.20
- Rust

## Installing Rust

### macOS & Linux
We recommend installing rust using rustup.
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### Windows
Please choose your preferred method of installation from [this page](https://forge.rust-lang.org/infra/other-installation-methods.html).

# Getting Started
You may refer to the example project found in the [examples](./examples/) directory. Note that you will need to have Rust installed on your system to compile and build this library.

## Library philosophy
This library is not a full port of MIDITok. Only the methods needed for inference have been reimplemented. As such, methods that interface with the HuggingFace Hub, dataloaders, data augmentations, and Byte Pair Encoding (BPE) training is not implemented.

The intended workflow is such that the tokenizer is prepared and tested in Python using MIDITok, and only imported to C++ applications once the tokenizer and the model are ready. The tokenizer can be imported from a config file, obtainable from MIDITok using the [.save() method on a tokenizer](https://miditok.readthedocs.io/en/latest/tokenizing_music_with_miditok.html#miditok.MusicTokenizer.save), and the model presumably via libtorch or equivalent.

## Adding LibMusicTok to your project using CMake FetchContent
Pull a version of this repository using a commit hash or a version tag, or from a local copy (as in the example project), make it available, and link your target to it.
You'll need to do the same for the project dependencies: [symusic](https://github.com/Yikai-Liao/symusic), [tokenizers_cpp](https://github.com/mlc-ai/tokenizers-cpp), and [json](https://github.com/nlohmann/json).

```cmake
FetchContent_Declare(
    LibMusicTok
    GIT_REPOSITORY https://github.com/steinbergmedia/libmusictok
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(LibMusicTok)
target_link_libraries(YourCoolProject PRIVATE LibMusicTok)
```

## Using the library
We recommend reading the inline docstrings inside [music_tokenizer.h](./include/libmusictok/tokenizations/music_tokenizer.h), [tok_sequence.h](./include/libmusictok/tok_sequence.h), [tokenizer_config.h](./include/libmusictok/tokenizer_config.h), and if needed, [utility_functions.h](./include/libmusictok/utility_functions.h)

```cpp
// Include a tokenizer architecture of your choice:
#include "libmusictok/tokenizations/remi.h"

// Create an instance of the tokenizer while passing in a path to a config file
auto pathToConfigFile = std::filesystem::path("path/to/config/file");
libmusictok::REMI tokenizer(pathToConfigFile);

// Alternatively, you can initialise a tokenizer by passing in a tokenizerConfig object
libmusictok::TokenizerConfig config;
config.useRests = true;
config.useTimeSignatures = false;

libmusictok::REMI tokenizer(config);

// NOTE:
// It is recommended that if you initialise a tokenizer from scratch in you code,
// you save your tokenizer configurations at this stage.
// As this library is built for inference rather than training, the config checks are
// more robust when being imported from a file.

auto pathToConfigFile = std::filesystem::path("path/to/config/file");
config.saveToJson(pathToConfigFile);


// You can now encode either a midi file or a symusic::Score object
// Note that if config.oneTokenStreamForPrograms is false, the encode method will return a vector<TokSequence>
// instead of a TokSequence

auto pathToInputMIDI = std::filesystem::path("path/to/midi/file");
auto tokenSequenceVariant = tokenizer.encode(pathToInputMIDI);
libmusictok::TokSequence tokenSequence;

if (tokenizer.config.oneTokenStreamForPrograms)
{
    tokenSequence = std::get<libmusictok::TokSequence>(tokenSequenceVariant);
}

// You can now get the ids or the tokens from this TokSequence object
std::vector<int> tokenIds = tokenSequence.ids.get();
std::vector<std::string> tokens = tokenSequence.tokens.get();

// If the tokenizer used has multiple vocabularies - uses pooling - which REMI does not,
// you need to use .getMultiVoc();

//--------------------------------------------------------------------------------------
// This is the part where you would then pass on the tokens to a model, and get new tokens
// out of it.
//--------------------------------------------------------------------------------------

// You can verify the sanity of a token sequence. This is useful to know if your token generator
// has failed to produce cohesive tokens. Accepts TokSequence and vectors of ints
float tokenSanity = tokenizer.tokensErrors(tokenSequence);
float tokenSanity = tokenizer.tokensErrors(tokenIds);

// You can decode a TokSequence object, or even a vector of integers directly.
symusic::Score<symusic::Tick> score = tokenizer.decode(tokenSequence);
symusic::Score<symusic::Tick> score = tokenizer.decode(tokenIds);
symusic::Score<symusic::Tick> score = tokenizer.decode(libmusictok::TokSequence(tokens));

// You can save the score back to a midi file either by passing an argument to the decode,
// or by using a utility function
libmusictokUtils::saveMidiFromScore(score, outputPath)

```

# Building this LibMusicTok

## Configuring Cmake :
Regardless of your OS, please run the following commands from the project root.

### macOS:
```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON
```
- **To build as an Xcode project**:
```bash
cmake -B buildXcode -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON -G Xcode
```

### Linux:
```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON
```

### Windows:
```bash
cmake -B build -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl -DCMAKE_BUILD_TYPE=Release -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON
```
- **To build as a Visual Studio solution**
```bash
cmake -B buildVs -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON -G "Visual Studio 17 2022"
```

## Build and Compile:
On all three platforms, the command is the same:
```bash
cmake --build build --config Release -j
```

## Run Tests and Benchmarks:

### macOS & Linux:
```bash
./build/LibMusicTokTest
./build/LibMusicTokBenchmark --benchmark_time_unit=ms
```

## Windows:
Assuming a bash shell:
```bash
./build/Release/LibMusicTokTest.exe
./build/Release/LibMusicTokBenchmark.exe --benchmark_time_unit=ms
```

# Docker
```bash
docker build -t libmusictok-app /path/to/repo
docker run libmusictok-app
```

# Acknowledgements
We would like to thank [Nathan Fradet](https://github.com/Natooz), author of [MIDITok](https://github.com/Natooz/MidiTok?tab=readme-ov-file), and [Paul Triana](https://github.com/DaoTwenty) for their help and support in getting this project off the ground.

This project makes use of core dependencies:
  - [symusic](https://github.com/Yikai-Liao/symusic) for it's compact and efficient processing of MIDI files into Score objects.
  - [tokenizers_cpp](https://github.com/mlc-ai/tokenizers-cpp) for it's porting of the huggingface tokenizers from Rust to C++.
  - [json](https://github.com/nlohmann/json) for reading tokenizer config files.
