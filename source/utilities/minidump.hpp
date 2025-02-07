#pragma once

#include "nt.hpp"

namespace exception
{
	std::string create_minidump(LPEXCEPTION_POINTERS exceptioninfo);
}
