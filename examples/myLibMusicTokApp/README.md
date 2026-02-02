# Example Application using LibMusicTok
This example app tokenizes and detokenizes a MIDI file given a tokenizer config path.

### Contents
  - src/: The source of the cpp code
  - CMakeLists.txt: The cmake config file to create your cpp project
  - midis/ and tokenizers/: folders for storing MIDI and tokenizer files. You may use them in the example
  - external/: The cmake files for the project dependencies.

## To build:
  - Follow the requirements on the main page README.
  - run `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release -j 16`

## To Run:
```bash
./build/MyLibMusicTokApp Path/to/midi path/to/.json <outputMidi>
```

## TakeAways:
To compile LibMusicTok in your application, you'll need a couple of things:
* The exact dependencies used:
  - Copy the external/ directory into yours
  - In your main CMakeLists, have the following:
  ```cmake
  # add_subdirectory(external)
  include(external/Dependencies.cmake)
  ```

* Fetching LibMusicTok using FetchContent: - see the CMakeLists lines 10-16
