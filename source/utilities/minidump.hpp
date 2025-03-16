#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

namespace utils
{
    class minidump final
    {
    public:
        static void initialize();
        static std::string create_crash_report(
#ifdef _WIN32
            LPEXCEPTION_POINTERS exceptionInfo
#else
            int signal_number,
            siginfo_t* signal_info,
            void* context
#endif
        );

    private:
        static void setup_signal_handlers();
    };
}
