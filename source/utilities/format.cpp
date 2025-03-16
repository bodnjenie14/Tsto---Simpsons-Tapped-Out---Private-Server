#include "format.hpp"
#include "string.hpp"

#define M_SECONDS 60
#define H_SECONDS 3600
#define D_SECONDS 86400

namespace utils::format
{
    static std::string result_buffer;

    const char* get_data_size_str(size_t bytes)
    {
        if (bytes < 1024)
        {
            result_buffer = string::va("%d B", bytes);
            return result_buffer.c_str();
        }

        const auto kb = bytes / 1024;
        if (kb < 1024)
        {
            result_buffer = string::va("%d KB", kb);
            return result_buffer.c_str();
        }

        const float mb = static_cast<float>(bytes) / (1024.0f * 1024.0f);
        result_buffer = string::va("%.2f MB", mb);
        return result_buffer.c_str();
    }

    const char* build_timelapse_str(uint32_t time)
    {
        const auto seconds = time % 60;
        const auto minutes = (time / 60) % 60;
        const auto hours = (time / 3600) % 24;
        const auto days = (time / 86400);

        if (days > 0)
        {
            result_buffer = string::va("%dD %dH %dM %dS", days, hours, minutes, seconds);
            return result_buffer.c_str();
        }

        if (hours > 0)
        {
            result_buffer = string::va("%dH %dM %dS", hours, minutes, seconds);
            return result_buffer.c_str();
        }

        if (minutes > 0)
        {
            result_buffer = string::va("%dM %dS", minutes, seconds);
            return result_buffer.c_str();
        }

        result_buffer = string::va("%dS", seconds);
        return result_buffer.c_str();
    }

    const char* format_timelapse_informal(unsigned int time)
    {
        const auto seconds = time % 60;
        const auto minutes = (time / 60) % 60;
        const auto hours = (time / 3600) % 24;
        const auto days = (time / 86400);

        if (seconds < 60 && minutes == 0 && hours == 0 && days == 0)
        {
            result_buffer = string::va("%d seconds ago", seconds);
        }
        else if (minutes < 60 && hours == 0 && days == 0)
        {
            result_buffer = (minutes > 1)
                ? string::va("%d minutes ago", minutes)
                : std::string("last minute");
        }
        else if (hours < 24 && days == 0)
        {
            result_buffer = (hours > 1)
                ? string::va("%d hours ago", hours)
                : std::string("last hour");
        }
        else
        {
            result_buffer = (days > 1)
                ? string::va("%d days ago", days)
                : std::string("last day");
        }

        return result_buffer.c_str();
    }
}
