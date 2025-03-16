#pragma once
#include <cstdint>
#include <thread>
#include <vector>
#include <functional>

namespace utils::thread
{
    // Get thread and process IDs
    uint32_t get_current_thread_id();
    uint32_t get_current_process_id();

    // Thread control functions
    void suspend_other_threads();
    void resume_other_threads();
    void sleep(uint32_t ms);

    // Process control
    bool kill_process(uint32_t pid);

    bool set_name(std::thread& t, const std::string& name);
    bool set_name(const std::string& name);

    template <typename ...Args>
    std::thread create_named_thread(const std::string& name, Args&&... args)
    {
        auto t = std::thread(std::forward<Args>(args)...);
        set_name(t, name);
        return t;
    }

    class handle
    {
    public:
        handle(const uint32_t thread_id, const uint32_t access = 0x1F0FFF)
            : handle_(nullptr)
        {
            #ifdef _WIN32
            this->handle_ = OpenThread(access, FALSE, thread_id);
            #else
            // Implement for other platforms
            #endif
        }

        operator bool() const
        {
            return this->handle_;
        }

        operator void*() const
        {
            return this->handle_;
        }

    private:
        void* handle_{};
    };

    std::vector<uint32_t> get_thread_ids();
    void for_each_thread(const std::function<void(void*)>& callback, uint32_t access = 0x1F0FFF);
}
