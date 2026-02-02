//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : examples/myLibMusicTokApp/src/main.cpp
// Created by  : Steinberg, 03/2025
// Description : tests the utility functions
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/tokenizations/mmm.h"
#include <iostream>

int runApp(int argc, char *argv[])
{
    std::cout << "Hello from myLibMusicTokApp!" << std::endl;
    if (argc < 3)
    {
        std::cout << "Please specify a MIDI file and a tokenizer.json of an MMM tokenizer." << std::endl;
    }

    if (argc >= 3)
    {
        std::string midiStr      = argv[1];
        std::string tokenizerStr = argv[2];
        std::filesystem::path midiPath(midiStr);
        std::filesystem::path tokenizerPath(tokenizerStr);

        libmusictok::MMM tokenizer = libmusictok::MMM(tokenizerPath);
        std::cout << "Tokenizing: " << midiStr << std::endl << std::endl;

        auto tokens = tokenizer.encode(midiPath);
        std::cout << "The first few tokens are: " << std::endl;

        libmusictok::TokSequence tokSeq   = std::get<libmusictok::TokSequence>(tokens);
        std::vector<std::string> tokenVec = tokSeq.tokens.get();
        std::vector<int> tokenIds         = tokSeq.ids.get();
        for (int i = 0; i < 10; ++i)
        {
            std::cout << tokenVec[i] << std::endl;
        }

        if (argc >= 4)
        {
            libmusictok::TokSequence tokSeqInput;
            tokSeqInput.ids                              = tokenIds;
            symusic::Score<symusic::Tick> tokenizedScore = tokenizer.decode(tokSeqInput);
            std::filesystem::path outputPath             = std::filesystem::path(argv[3]);
            libmusictokUtils::saveMidiFromScore(tokenizedScore, outputPath);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    return runApp(argc, argv);
}
