#pragma once
#include <string>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
typedef void* HANDLE;
#endif

namespace debugging
{
    class console
    {
    public:
        console();
        ~console();

        void set_color(int color);
        void write(const char* message);
        void write(const std::string& message);
        int get_window_width();

    private:
#ifdef _WIN32
        HANDLE handle_;
        int default_attributes_;
#else
        int default_attributes_;
#endif
    };

    void create_console();
    void close_console();
} // namespace debugging
