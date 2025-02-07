#include <std_include.hpp>

#include <windows.h>
#include <iostream>
#include <cstdio>
#include "console.hpp"
#include "serverlog.hpp"


void create_console()
{
    AllocConsole();  

    FILE* stream;

    freopen_s(&stream, "CONOUT$", "w", stdout);

    freopen_s(&stream, "CONOUT$", "w", stderr);

    freopen_s(&stream, "CONIN$", "r", stdin);

    SetConsoleTitleA("Bods Server Debug Console");

    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow != nullptr)
    {
        RECT consoleRect;
        GetWindowRect(consoleWindow, &consoleRect);
        MoveWindow(consoleWindow, consoleRect.left, consoleRect.top, 800, 600, TRUE);
    }

    std::cout.clear();
    std::cerr.clear();

    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Bods Evpp Server Console Started");
}
