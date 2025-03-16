#include "std_include.hpp"
#include "serverlog.hpp"
#include "utilities/string.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#endif

namespace logger
{
    const char* LogLevelNames[] =
    {
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR",
        "CRITICAL",
        "INCOMING",
        "RESPONSE",
        "PLAYERID"
    };

    const char* LogLabelNames[] =
    {
        "INITIALIZER",
        "BLACKBOX",
        "DISPATCH::TCP",
        "DISPATCH::UDP",
        "SERVER::HTTP",
        "TRACKING",  
        "GAME",
        "LOBBY",
        "AUTH",
        "PIN",
        "USER",
        "LAND",
        "SESSION",
        "FILESERVER",
        "EVENTS",
		"DISCORD",
		"UPDATE",
		"DATABASE",
    };

    inline const char* get_log_level_str(LogLevel lvl)
    {
        return LogLevelNames[lvl];
    }

    inline const char* get_log_label_str(LogLabel lbl)
    {
        return LogLabelNames[lbl];
    }

    void write(const char* file, std::string str)
    {
        std::ofstream stream;
        stream.open(file, std::ios_base::app);
        //stream.write(str.data(),str.length());
        stream << str << std::endl;
    }

    void write(LogLevel level, LogLabel label, const char* fmt, ...) {
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
#ifdef _WIN32
        vsprintf_s(buffer, sizeof(buffer), fmt, args);
#else
        vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
        va_end(args);

        const auto now = std::chrono::system_clock::now();
        const auto now_time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << "] ";
        
        // Add log level prefix
        switch (level) {
            case LOG_LEVEL_DEBUG:
                ss << "[DEBUG] ";
                break;
            case LOG_LEVEL_INFO:
                ss << "[INFO] ";
                break;
            case LOG_LEVEL_WARN:
                ss << "[WARNING] ";
                break;
            case LOG_LEVEL_ERROR:
                ss << "[ERROR] ";
                break;
            case LOG_LEVEL_CRITICAL:
                ss << "[CRITICAL] ";
                break;
            case LOG_LEVEL_INCOMING:
                ss << "[INCOMING] ";
                break;
            case LOG_LEVEL_RESPONSE:
                ss << "[RESPONSE] ";
                break;
            case LOG_LEVEL_PLAYER_ID:
                ss << "[PLAYER_ID] ";
                break;
        }

        // Add label prefix
        if (label >= 0 && label < sizeof(LogLabelNames) / sizeof(LogLabelNames[0])) {
            ss << "[" << LogLabelNames[label] << "] ";
        } else {
            ss << "[UNKNOWN] ";
        }

        ss << buffer;

#ifdef _WIN32
        // Set console color based on log level
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            WORD attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default white

            switch (level) {
                case LOG_LEVEL_ERROR:
                case LOG_LEVEL_CRITICAL:
                    attributes = FOREGROUND_RED | FOREGROUND_INTENSITY;
                    break;
                case LOG_LEVEL_WARN:
                    attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                    break;
                case LOG_LEVEL_INFO:
                    attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                    break;
                case LOG_LEVEL_DEBUG:
                    attributes = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                    break;
            }

            SetConsoleTextAttribute(hConsole, attributes);
            std::cout << ss.str() << std::endl;
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        } else {
            std::cout << ss.str() << std::endl;
        }

        OutputDebugStringA((ss.str() + "\n").c_str());
#else
        // For Linux/Docker, always ensure we have a newline and flush
        std::cout << ss.str() << std::endl;
        std::cout.flush();
#endif

        // Write to log file
        std::ofstream logfile("server.log", std::ios::app);
        if (logfile.is_open()) {
            logfile << ss.str() << std::endl;
            logfile.close();
        }
    }

    void log_packet_buffer(const char* stub, const char* buffer, size_t length) {
        std::stringstream ss;
        ss << stub << " ----> " << "length:" << length << " hexilify: " << utils::string::dump_hex(std::string(buffer, length));

        std::ofstream stream;
        stream.open("dw-emulator.log", std::ios_base::app);
        if (stream.is_open()) {
            stream << ss.str() << std::endl;
            stream.close();
        }

#ifdef _WIN32
        OutputDebugStringA(ss.str().c_str());
#endif
        std::cout << ss.str() << std::endl;
        std::cout.flush();
    }
}