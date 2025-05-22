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
        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // Calculate position to center the console window
        int consoleWidth = 800;
        int consoleHeight = 600;
        int posX = (screenWidth - consoleWidth) / 2;
        int posY = (screenHeight - consoleHeight) / 2;

        // Position the console window in the center of the screen
        MoveWindow(consoleWindow, posX, posY, consoleWidth, consoleHeight, TRUE);
    }

    std::cout.clear();
    std::cerr.clear();

    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Bods Evpp Server Console Started");
}
