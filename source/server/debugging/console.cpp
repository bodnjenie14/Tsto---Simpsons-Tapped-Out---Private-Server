#include "std_include.hpp"
#include "console.hpp"
#include "serverlog.hpp"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

namespace debugging {
    void create_console() {
#ifdef _WIN32
        if (AllocConsole()) {
            // Redirect standard input/output streams
            FILE* dummy;
            freopen_s(&dummy, "CONIN$", "r", stdin);
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);

            // Set console title
            SetConsoleTitleA("TSTO Server Debug Console");

            // Enable ANSI escape sequences
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dwMode = 0;
            GetConsoleMode(hOut, &dwMode);
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
#else
        // For Linux, ensure stdout is properly configured for Docker
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        std::cout << "TSTO Server Console Started in Linux environment" << std::endl;
#endif
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "TSTO Server Console Started");
    }

    void close_console() {
#ifdef _WIN32
        FreeConsole();
#endif
    }
}
