#pragma once
#include "string.hpp"
#include <cstdint>

namespace utils::format
{
	const char* get_data_size_str(size_t bytes);
	const char* build_timelapse_str(uint32_t seconds);
	const char* format_timelapse_informal(unsigned int seconds);
}
