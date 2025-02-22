#include <std_include.hpp>
#include "serverlog.hpp"
#include <string.hpp>
#include <cstdio>

#define OUTPUT_DEBUG_API
#define PREPEND_TIMESTAMP

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

    void write(LogLevel level, LogLabel label, const char* fmt, ...)
    {
        char va_buffer[85768] = { 0 }; // Increased from 0x1000 to 8192

        va_list ap;
        va_start(ap, fmt);
        vsprintf_s(va_buffer, fmt, ap);
        va_end(ap);

        std::stringstream stream;

#ifdef PREPEND_TIMESTAMP
        time_t now = time(0);
        std::tm* t = std::localtime(&now);
        stream << "" << std::put_time(t, "%Y-%m-%d %H:%M:%S") << "\t";
#endif // PREPEND_TIMESTAMP

        stream << "[ " << get_log_level_str(level) << " ]";
        if (label != -1) {
            stream << "[ " << get_log_label_str(label) << " ]";
        }
        stream << " " << va_buffer;

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE)
        {
            // colors based on log levels
            switch (level)
            {
            case LOG_LEVEL_ERROR:
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY); // Red
                break;

            case LOG_LEVEL_WARN:
                SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Green
                break;

            case LOG_LEVEL_INCOMING:
                SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY); // Cyan-green
                break;
            case LOG_LEVEL_RESPONSE:
                SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY); // Blue
                break;
            default:
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Default white
                break;
            case LOG_LEVEL_PLAYER_ID:
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Yellow
                break;

            }

            std::cout << stream.str() << std::endl;

            // Reset color 
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }

#ifdef OUTPUT_DEBUG_API
        OutputDebugStringA(stream.str().c_str());
#endif // OUTPUT_DEBUG_API

        write("tsto_server.log", stream.str());
    }

    void log_packet_buffer(const char* stub, const char* buffer, size_t length)
    {
        std::stringstream ss;
        ss << stub << " ----> " << "length:" << length << " hexilify: " << utils::string::dump_hex(std::string(buffer, length));

        std::ofstream stream;
        stream.open("dw-emulator.log", std::ios_base::app);

#ifdef PREPEND_TIMESTAMP
        time_t now = time(0);
        std::tm* t = std::localtime(&now);
        stream << "" << std::put_time(t, "%Y-%m-%d %H:%M:%S") << "\t";
#endif // PREPEND_TIMESTAMP

#ifdef OUTPUT_DEBUG_API
        OutputDebugStringA(ss.str().c_str());
#endif // OUTPUT_DEBUG_API

        stream << ss.str() << std::endl;
    }
}