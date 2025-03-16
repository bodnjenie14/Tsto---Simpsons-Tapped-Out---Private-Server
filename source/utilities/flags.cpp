#include "flags.hpp"
#include "string.hpp"
#include "nt.hpp"

#ifdef _WIN32
#include <shellapi.h>
#endif

namespace utils::flags
{
        namespace
        {
                std::vector<std::string> enabled_flags;
                bool parsed = false;

#ifdef _WIN32
                void parse_flags_windows(std::vector<std::string>& flags)
                {
                        int num_args;
                        auto* const argv = CommandLineToArgvW(GetCommandLineW(), &num_args);

                        flags.clear();

                        if (argv)
                        {
                                for (auto i = 0; i < num_args; ++i)
                                {
                                        std::wstring wide_flag(argv[i]);
                                        if (wide_flag[0] == L'-')
                                        {
                                                wide_flag.erase(wide_flag.begin());
                                                flags.emplace_back(string::convert(wide_flag));
                                        }
                                }

                                LocalFree(argv);
                        }
                }
#endif

                void parse_flags_posix(std::vector<std::string>& flags, int argc, char* argv[])
                {
                        flags.clear();
                        for (int i = 1; i < argc; ++i)
                        {
                                std::string arg(argv[i]);
                                if (!arg.empty() && arg[0] == '-')
                                {
                                        flags.emplace_back(arg.substr(1));
                                }
                        }
                }
        }

        void parse_flags(std::vector<std::string>& flags, int argc, char* argv[])
        {
#ifdef _WIN32
                parse_flags_windows(flags);
#else
                parse_flags_posix(flags, argc, argv);
#endif
        }

        bool has_flag(const std::string& flag)
        {
                if (!parsed)
                {
                        // Default to empty flags if not explicitly initialized
                        enabled_flags.clear();
                        parsed = true;
                }

                for (const auto& entry : enabled_flags)
                {
                        if (string::to_lower(entry) == string::to_lower(flag))
                        {
                                return true;
                        }
                }

                return false;
        }

        void set_flags(int argc, char* argv[])
        {
                parse_flags(enabled_flags, argc, argv);
                parsed = true;
        }
}
