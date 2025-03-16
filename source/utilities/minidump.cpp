#include "minidump.hpp"
#include "compression.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <signal.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <cstring>
#endif

namespace utils
{
    namespace
    {
#ifdef _WIN32
        LONG WINAPI unhandled_exception_handler(PEXCEPTION_POINTERS exception_info)
        {
            std::string crash_report = minidump::create_crash_report(exception_info);
            
            // Save crash report
            std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
            std::string crash_file = (temp_dir / "crash.txt").string();
            
            std::ofstream report(crash_file);
            if (report)
            {
                report << crash_report;
                report.close();
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }
#else
        void signal_handler(int sig, siginfo_t* info, void* context)
        {
            std::string crash_report = minidump::create_crash_report(sig, info, context);
            
            // Save crash report
            std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
            std::string crash_file = (temp_dir / "crash.txt").string();
            
            std::ofstream report(crash_file);
            if (report)
            {
                report << crash_report;
                report.close();
            }

            // Re-raise the signal for default handling
            signal(sig, SIG_DFL);
            raise(sig);
        }

        std::string get_backtrace()
        {
            std::stringstream trace;
            void* array[50];
            int size = backtrace(array, 50);
            char** messages = backtrace_symbols(array, size);

            trace << "Backtrace:\n";
            for (int i = 0; i < size && messages != nullptr; ++i)
            {
                char* mangled_name = nullptr;
                char* offset_begin = nullptr;
                char* offset_end = nullptr;

                // Find parentheses and +address offset surrounding mangled name
                for (char* p = messages[i]; *p; ++p)
                {
                    if (*p == '(')
                    {
                        mangled_name = p;
                    }
                    else if (*p == '+')
                    {
                        offset_begin = p;
                    }
                    else if (*p == ')')
                    {
                        offset_end = p;
                        break;
                    }
                }

                // If the line could be processed, attempt to demangle the symbol
                if (mangled_name && offset_begin && offset_end && mangled_name < offset_begin)
                {
                    *mangled_name++ = '\0';
                    *offset_begin++ = '\0';
                    *offset_end = '\0';

                    int status = 0;
                    char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
                    
                    trace << "[" << i << "]: " << messages[i] << "("
                          << (status == 0 ? demangled : mangled_name)
                          << "+" << offset_begin << ")\n";

                    free(demangled);
                }
                else
                {
                    trace << "[" << i << "]: " << messages[i] << "\n";
                }
            }

            free(messages);
            return trace.str();
        }
#endif
    }

    void minidump::initialize()
    {
#ifdef _WIN32
        SetUnhandledExceptionFilter(unhandled_exception_handler);
#else
        struct sigaction sa;
        sa.sa_sigaction = signal_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
        sigaction(SIGILL, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
#endif
    }

    std::string minidump::create_crash_report(
#ifdef _WIN32
        LPEXCEPTION_POINTERS exception_info
#else
        int signal_number,
        siginfo_t* signal_info,
        void* context
#endif
    )
    {
        std::stringstream crash_report;
        
        // Add timestamp
        std::time_t now = std::time(nullptr);
        crash_report << "Crash Report - " << std::ctime(&now) << "\n";

#ifdef _WIN32
        // Windows-specific crash info
        EXCEPTION_RECORD* exception = exception_info->ExceptionRecord;
        crash_report << "Exception Code: 0x" << std::hex << exception->ExceptionCode << "\n"
                    << "Exception Address: 0x" << exception->ExceptionAddress << "\n\n";

        // Create minidump using dbghelp
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
        std::string dump_file = (temp_dir / "crash.dmp").string();

        HANDLE file = CreateFileA(dump_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (file != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = exception_info;
            mei.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                            MiniDumpNormal, &mei, nullptr, nullptr);
            CloseHandle(file);

            // Compress the dump file
            compression::compress_file(dump_file, dump_file + ".zip");
            std::filesystem::remove(dump_file);
            
            crash_report << "Minidump saved to: " << dump_file << ".zip\n";
        }
#else
        // Linux-specific crash info
        crash_report << "Signal: " << strsignal(signal_number) << " (" << signal_number << ")\n";
        if (signal_info)
        {
            crash_report << "Signal Code: " << signal_info->si_code << "\n"
                        << "Faulting Address: " << signal_info->si_addr << "\n\n";
        }

        // Add backtrace
        crash_report << get_backtrace();
#endif

        return crash_report.str();
    }
}
