/* Copyright 2016 Pete Warden. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ==============================================================================*/

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "picoproto.h"

namespace picoproto {
namespace {

template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "Sizes do not match");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

#define PP_EXPECT(X)                                               \
  if (!(X)) {                                                      \
    PP_LOG(ERROR) << #X << " expected to be true, but was " << (X) \
                  << std::endl;                                    \
    return false;                                                  \
  }

#define PP_EXPECT_EQ(X, Y)                                                 \
  if ((X) != (Y)) {                                                        \
    PP_LOG(ERROR) << #Y << " expected to be " << #X << ", but was " << (Y) \
                  << std::endl;                                            \
    return false;                                                          \
  }

#define PP_EXPECT_NE(X, Y)                                             \
  if ((X) == (Y)) {                                                    \
    PP_LOG(ERROR) << #Y << " expected not to be " << #X << ", but was" \
                  << std::endl;                                        \
    return false;                                                      \
  }

#define PP_EXPECT_NEAR(X, Y, E)                                            \
  {                                                                        \
    double delta = ((X) - (Y));                                            \
    if (delta < 0) {                                                       \
      delta = -delta;                                                      \
    }                                                                      \
    if (delta > (E)) {                                                     \
      PP_LOG(ERROR) << #Y << " expected to be near " << #X << ", but was " \
                    << (Y) << std::endl;                                   \
      return false;                                                        \
    }                                                                      \
  }

void TestFunction(std::string name, std::function<bool()> test_func) {
  const bool test_result = test_func();
  if (test_result) {
    std::cerr << name << " : PASSED" << std::endl;
  } else {
    std::cerr << name << " : FAILED" << std::endl;
  }
}

void TestAll() {
  TestFunction("Simple", [] {
    std::vector<uint8_t> bytes({0x08, 0x96, 0x01});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ(150, message.GetUInt64(1));
    return true;
  });

  TestFunction("String", [] {
    std::vector<uint8_t> bytes(
        {0x12, 0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ("testing", message.GetString(2));
    return true;
  });

  TestFunction("StringNoCopy", [] {
    std::vector<uint8_t> bytes(
        {0x12, 0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67});
    Message message(false);
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ("testing", message.GetString(2));
    bytes[2] = 'p';
    PP_EXPECT_EQ("pesting", message.GetString(2));
    return true;
  });

  TestFunction("EmbeddedMessage", [] {
    std::vector<uint8_t> bytes({0x1a, 0x03, 0x08, 0x96, 0x01});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    Message* embedded_message = message.GetMessage(3);
    PP_EXPECT_NE(nullptr, embedded_message);
    PP_EXPECT_EQ(150, embedded_message->GetUInt64(1));
    return true;
  });

  TestFunction("RepeatedVarInt", [] {
    std::vector<uint8_t> bytes({0x08, 0x96, 0x01, 0x08, 0x7f});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ(150, message.GetUInt64(1));
    std::vector<uint64_t> int_array = message.GetUInt64Array(1);
    PP_EXPECT_EQ(2, int_array.size());
    PP_EXPECT_EQ(150, int_array[0]);
    PP_EXPECT_EQ(127, int_array[1]);
    return true;
  });

  TestFunction("NegativeVarInt", [] {
    std::vector<uint8_t> bytes({0x08, 0x03});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ(-2, message.GetInt64(1));
    return true;
  });
  
  TestFunction("UInt32", [] {
    std::vector<uint8_t> bytes({0x0d, 0x01, 0x00, 0x00, 0x00});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ(1, message.GetUInt32(1));
    return true;
  });

  TestFunction("UInt64", [] {
    std::vector<uint8_t> bytes(
        {0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_EQ(1, message.GetUInt64(1));
    return true;
  });

  TestFunction("Float", [] {
    std::vector<uint8_t> bytes({0x0d});
    const float value = -42.42f;
    uint32_t value_as_int = bit_cast<uint32_t>(value);
    bytes.push_back((value_as_int >> 0) & 0xff);
    bytes.push_back((value_as_int >> 8) & 0xff);
    bytes.push_back((value_as_int >> 16) & 0xff);
    bytes.push_back((value_as_int >> 24) & 0xff);
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_NEAR(-42.42f, message.GetFloat(1), 1e-5f);
    return true;
  });

  TestFunction("Double", [] {
    std::vector<uint8_t> bytes({0x09});
    const double value = 23.23;
    uint64_t value_as_int = bit_cast<uint64_t>(value);
    bytes.push_back((value_as_int >> 0) & 0xff);
    bytes.push_back((value_as_int >> 8) & 0xff);
    bytes.push_back((value_as_int >> 16) & 0xff);
    bytes.push_back((value_as_int >> 24) & 0xff);
    bytes.push_back((value_as_int >> 32) & 0xff);
    bytes.push_back((value_as_int >> 40) & 0xff);
    bytes.push_back((value_as_int >> 48) & 0xff);
    bytes.push_back((value_as_int >> 56) & 0xff);
    Message message;
    message.ParseFromBytes(bytes.data(), bytes.size());
    PP_EXPECT_NEAR(23.23f, message.GetDouble(1), 1e-5f);
    return true;
  });
}

}  // namespace
}  // namespace picoproto

int main(int argc, const char* argv[]) {
  picoproto::TestAll();
  return 0;
}
