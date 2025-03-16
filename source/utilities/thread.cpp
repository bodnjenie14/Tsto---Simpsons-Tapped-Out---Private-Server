#include "thread.hpp"
#include <thread>
#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#endif

namespace utils::thread
{
    namespace
    {
#ifdef _WIN32
        void close_handle(HANDLE handle)
        {
            if (handle)
            {
                CloseHandle(handle);
            }
        }
#endif
    }

    uint32_t get_current_thread_id()
    {
#ifdef _WIN32
        return static_cast<uint32_t>(GetCurrentThreadId());
#else
        return static_cast<uint32_t>(gettid());
#endif
    }

    uint32_t get_current_process_id()
    {
#ifdef _WIN32
        return static_cast<uint32_t>(GetCurrentProcessId());
#else
        return static_cast<uint32_t>(getpid());
#endif
    }

    void suspend_other_threads()
    {
#ifdef _WIN32
        const auto current_thread = GetCurrentThread();
        const auto current_thread_id = GetCurrentThreadId();
        const auto current_process = GetCurrentProcessId();

        HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (h != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            if (Thread32First(h, &te))
            {
                do
                {
                    if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
                    {
                        if (te.th32OwnerProcessID == current_process && te.th32ThreadID != current_thread_id)
                        {
                            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                            if (thread)
                            {
                                SuspendThread(thread);
                                CloseHandle(thread);
                            }
                        }
                    }
                    te.dwSize = sizeof(te);
                } while (Thread32Next(h, &te));
            }
            CloseHandle(h);
        }
#else
        // On Linux, we don't suspend other threads as it's not safe
        // Instead, we could use signals but that's generally not recommended
#endif
    }

    void resume_other_threads()
    {
#ifdef _WIN32
        const auto current_thread = GetCurrentThread();
        const auto current_thread_id = GetCurrentThreadId();
        const auto current_process = GetCurrentProcessId();

        HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (h != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            if (Thread32First(h, &te))
            {
                do
                {
                    if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
                    {
                        if (te.th32OwnerProcessID == current_process && te.th32ThreadID != current_thread_id)
                        {
                            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                            if (thread)
                            {
                                ResumeThread(thread);
                                CloseHandle(thread);
                            }
                        }
                    }
                    te.dwSize = sizeof(te);
                } while (Thread32Next(h, &te));
            }
            CloseHandle(h);
        }
#else
        // On Linux, since we don't suspend threads, we don't need to resume them
#endif
    }

    void sleep(uint32_t ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    bool kill(uint32_t pid)
    {
#ifdef _WIN32
        auto* const handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!handle) return false;

        const auto _ = gsl::finally([handle]()
        {
            close_handle(handle);
        });

        return TerminateProcess(handle, 0);
#else
        return ::kill(static_cast<pid_t>(pid), SIGTERM) == 0;
#endif
    }
}
