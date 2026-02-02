//------------------------------------------------------------------------
// Project     : libmusictok
//
// Category    : Tests
// Filename    : test/tokenizers_cpp_test.cpp
// Created by  : Steinberg, 03/2025
// Description : Tests the imported tokenizers-cpp library
//
//------------------------------------------------------------------------
// This file is part of libmusictok. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/libmusictok/LICENSE
//------------------------------------------------------------------------

#include "libmusictok/utility_functions.h"
#include "testing_utilities.hpp"
#include <filesystem>
#include <gtest/gtest.h>

//------------------------------------------------------------------------

#include <tokenizers_c.h>
#include <tokenizers_cpp.h>

//------------------------------------------------------------------------
namespace libmusictokTest {

//------------------------------------------------------------------------
class TokenizersCppTest : public ::testing::Test
{
protected:
    TokenizersCppTest() {}
    virtual void SetUp() {}
    std::filesystem::path hfTokenizerExample =
        std::filesystem::path(resourcesPath) / "hf_tokenizers" / "hf_tokenizer_example.json";
};

//------------------------------------------------------------------------
TEST_F(TokenizersCppTest, usingTokenizers)
{
    // Read blob from file.
    nlohmann::json blob = libmusictokUtils::loadJson(hfTokenizerExample).dump();

    auto tok = tokenizers::Tokenizer::FromBlobJSON(blob);

    std::string prompt = "What is the capital of Canada?";
    // call Encode to turn prompt into token ids
    std::vector<int> ids = tok->Encode(prompt);
    // call Decode to turn ids into string
    std::string decoded_prompt = tok->Decode(ids);

    std::string expectedTokenisation = "What is the capital of Canada?";
    ASSERT_EQ(expectedTokenisation, decoded_prompt);
}

} // namespace libmusictokTest
