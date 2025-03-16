#pragma once

#include <string>
#include <vector>

namespace utils::string
{
    // String manipulation functions
    std::string to_lower(const std::string& text);
    std::string to_upper(const std::string& text);
    std::vector<std::string> split(const std::string& text, char delimiter);
    bool starts_with(const std::string& text, const std::string& substring);
    bool ends_with(const std::string& text, const std::string& substring);
    bool contains(const std::string& text, const std::string& substring);
    std::string trim(const std::string& text);
    std::string replace_all(std::string text, const std::string& from, const std::string& to);

    // Hex conversion
    std::string dump_hex(const std::string& data, const std::string& separator = " ");

    // Platform-specific clipboard operations
    std::string get_clipboard_data();
    void set_clipboard_data(const std::string& data);

    // Legacy compatibility functions
    void strip(const char* in, char* out, int max);
    void strip(std::string& text);
    std::string va(const char* fmt, ...);
}
