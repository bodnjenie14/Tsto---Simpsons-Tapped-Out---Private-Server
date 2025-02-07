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

#include <stdio.h>

#include <iostream>
#include <vector>

#include "picoproto.h"

const int32_t GRAPH_DEF_NODE_FIELD = 1;
const int32_t NODE_DEF_NAME_FIELD = 1;
const int32_t NODE_DEF_OP_FIELD = 2;
const int32_t NODE_DEF_INPUT_FIELD = 3;

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    PP_LOG(ERROR) << "No file name found in arguments" << std::endl;
    return -1;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) {
    PP_LOG(ERROR) << "Couldn't find file " << argv[1] << std::endl;
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t* bytes = new uint8_t[fsize];
  fread(bytes, fsize, 1, f);
  fclose(f);

  picoproto::Message graph_def;
  graph_def.ParseFromBytes(bytes, fsize);

  std::vector<picoproto::Message*> nodes =
      graph_def.GetMessageArray(GRAPH_DEF_NODE_FIELD);

  std::cout << "digraph graphname {\n";
  for (picoproto::Message* node : nodes) {
    std::string node_name = node->GetString(NODE_DEF_NAME_FIELD);
    std::string op_name = node->GetString(NODE_DEF_OP_FIELD);
    std::cout << "  \"" << node_name << "\" [label=\"" << op_name << "\"];\n";
    std::vector<std::string> inputs =
        node->GetStringArray(NODE_DEF_INPUT_FIELD);
    for (const std::string& input_name : inputs) {
      size_t colon_pos = input_name.find(":");
      if (colon_pos == std::string::npos) {
        colon_pos = input_name.size();
      }
      size_t hat_pos;
      if (input_name[0] == '^') {
        hat_pos = 1;
      } else {
        hat_pos = 0;
      }
      std::string input_node_name =
          input_name.substr(hat_pos, colon_pos - hat_pos);
      std::cout << "  \"" << input_node_name << "\" -> \"" << node_name
                << "\";\n";
    }
  }
  std::cout << "}\n";

  delete[] bytes;
  return 0;
}
