#pragma once

#include <string>
#include <vector>

namespace utils::flags
{
        void parse_flags(std::vector<std::string>& flags, int argc, char* argv[]);
        bool has_flag(const std::string& flag);
        void set_flags(int argc, char* argv[]);
}
