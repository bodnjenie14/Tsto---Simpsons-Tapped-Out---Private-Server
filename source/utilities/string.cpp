#include "string.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#endif

namespace utils::string
{
    std::string va(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);

        char buffer[0x1000];
        const auto res = vsnprintf(buffer, sizeof(buffer), fmt, ap);

        va_end(ap);

        if (res > 0) return std::string(buffer, std::min(static_cast<size_t>(res), sizeof(buffer)));
        return {};
    }

    std::string to_lower(const std::string& text)
    {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    std::string to_upper(const std::string& text)
    {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    std::string dump_hex(const std::string& data, const std::string& separator)
    {
        std::ostringstream s;
        s << std::hex << std::setfill('0');
        for (unsigned char ch : data)
        {
            s << std::setw(2) << static_cast<unsigned>(ch) << separator;
        }
        return s.str();
    }

    std::vector<std::string> split(const std::string& s, const char delim)
    {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delim))
        {
            if (!item.empty())
            {
                result.push_back(item);
            }
        }

        return result;
    }

    std::vector<std::string> split(const std::string& s, const std::string& delim)
    {
        std::vector<std::string> result;
        size_t pos_start = 0;
        size_t pos_end;
        const size_t delim_len = delim.length();

        while ((pos_end = s.find(delim, pos_start)) != std::string::npos)
        {
            std::string token = s.substr(pos_start, pos_end - pos_start);
            if (!token.empty())
            {
                result.push_back(token);
            }
            pos_start = pos_end + delim_len;
        }

        std::string token = s.substr(pos_start);
        if (!token.empty())
        {
            result.push_back(token);
        }

        return result;
    }

    bool starts_with(const std::string& text, const std::string& substring)
    {
        return text.rfind(substring, 0) == 0;
    }

    bool ends_with(const std::string& text, const std::string& substring)
    {
        if (substring.size() > text.size()) return false;
        return std::equal(substring.rbegin(), substring.rend(), text.rbegin());
    }

    std::string replace(std::string str, const std::string& from, const std::string& to)
    {
        if (from.empty()) return str;

        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos)
        {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }

        return str;
    }

    std::string get_clipboard_data()
    {
#ifdef _WIN32
        if (OpenClipboard(nullptr))
        {
            std::string result;
            auto* const clipboard_data = GetClipboardData(CF_TEXT);
            if (clipboard_data)
            {
                auto* const cliptext = static_cast<char*>(GlobalLock(clipboard_data));
                if (cliptext)
                {
                    result = cliptext;
                    GlobalUnlock(clipboard_data);
                }
            }
            CloseClipboard();
            return result;
        }
#endif
        return {};
    }

    void set_clipboard_data(const std::string& text)
    {
#ifdef _WIN32
        if (OpenClipboard(nullptr))
        {
            EmptyClipboard();
            const auto size = text.size() + 1;
            auto* const mem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (mem)
            {
                auto* const data = static_cast<char*>(GlobalLock(mem));
                if (data)
                {
                    memcpy(data, text.data(), size);
                    GlobalUnlock(mem);
                    SetClipboardData(CF_TEXT, mem);
                }
            }
            CloseClipboard();
        }
#endif
    }

    bool match_compare(const std::string& input, const std::string& text, const bool exact)
    {
        if (exact && text == input) return true;
        if (!exact && text.find(input) != std::string::npos) return true;
        return false;
    }
}
