#include "memory.hpp"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace utils
{
    namespace
    {
        bool is_readable_memory(const void* ptr, size_t size)
        {
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi = {};
            if (!VirtualQuery(ptr, &mbi, sizeof(mbi))) return false;

            const DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);

            return ptr && ptr != (void*)0xFFFFFFFF && (mbi.Protect & mask) != 0;
#else
            // Try to read the memory without causing a segfault
            if (!ptr || size == 0) return false;

            unsigned char buffer[4096];
            const unsigned char* p = static_cast<const unsigned char*>(ptr);
            
            // Check page by page
            while (size > 0) {
                size_t to_read = (size > sizeof(buffer)) ? sizeof(buffer) : size;
                if (msync(const_cast<void*>(static_cast<const void*>(p)), to_read, MS_ASYNC) != 0) {
                    return false;
                }
                p += to_read;
                size -= to_read;
            }
            return true;
#endif
        }

        size_t get_allocation_size(void* ptr)
        {
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(ptr, &mbi, sizeof(mbi)))
            {
                return mbi.RegionSize;
            }
            return 0;
#else
            // On Linux, we need to get the page size
            return sysconf(_SC_PAGESIZE);
#endif
        }
    }

    memory::allocator::~allocator()
    {
        this->clear();
    }

    void memory::allocator::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (void* ptr : pool_)
        {
            memory::free(ptr);
        }
        pool_.clear();
    }

    void memory::allocator::free(void* data)
    {
        if (!data) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(pool_.begin(), pool_.end(), data);
        if (it != pool_.end())
        {
            memory::free(data);
            pool_.erase(it);
        }
    }

    void memory::allocator::free(const void* data)
    {
        this->free(const_cast<void*>(data));
    }

    void memory::allocator::free_all()
    {
        this->clear();
    }

    void* memory::allocator::allocate(size_t size)
    {
        if (size == 0) return nullptr;

        void* data = memory::allocate(size);
        if (data)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push_back(data);
        }
        return data;
    }

    bool memory::allocator::find(const void* data) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::find(pool_.begin(), pool_.end(), const_cast<void*>(data)) != pool_.end();
    }

    char* memory::allocator::duplicate_string(const std::string& string)
    {
        if (string.empty()) return nullptr;

        const size_t size = string.length() + 1;
        char* data = static_cast<char*>(this->allocate(size));
        if (data)
        {
            std::memcpy(data, string.c_str(), size);
        }
        return data;
    }

    bool memory::is_bad_read_ptr(const void* ptr)
    {
        if (!ptr || ptr == (void*)0xFFFFFFFF) return true;
        return !is_readable_memory(ptr, 1);
    }

    bool memory::is_bad_code_ptr(const void* ptr)
    {
        return is_bad_read_ptr(ptr);
    }

    bool memory::is_rdata_ptr(void* ptr)
    {
        return !is_bad_read_ptr(ptr);
    }

    void* memory::allocate(size_t size)
    {
        if (size == 0) return nullptr;

#ifdef _WIN32
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        // Round up to page size
        size_t pageSize = sysconf(_SC_PAGESIZE);
        size_t roundedSize = (size + pageSize - 1) & ~(pageSize - 1);
        void* ptr = mmap(nullptr, roundedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (ptr != MAP_FAILED) ? ptr : nullptr;
#endif
    }

    void memory::free(void* data)
    {
        if (!data) return;

#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        size_t size = get_allocation_size(data);
        if (size > 0)
        {
            munmap(data, size);
        }
#endif
    }

    void memory::free(const void* data)
    {
        free(const_cast<void*>(data));
    }

    bool memory::is_set(const void* mem, char chr, size_t length)
    {
        if (is_bad_read_ptr(mem) || length == 0) return false;
        const char* str = static_cast<const char*>(mem);
        return std::all_of(str, str + length, [chr](char c) { return c == chr; });
    }

    void* memory::copy(void* destination, const void* source, size_t length)
    {
        if (is_bad_read_ptr(source) || !destination || length == 0) return nullptr;
        return std::memcpy(destination, source, length);
    }

    void* memory::set(void* ptr, char chr, size_t length)
    {
        if (!ptr || length == 0) return nullptr;
        return std::memset(ptr, chr, length);
    }

    char* memory::duplicate_string(const std::string& string)
    {
        return get_allocator()->duplicate_string(string);
    }

    memory::allocator memory::mem_allocator_;

    memory::allocator* memory::get_allocator()
    {
        return &memory::mem_allocator_;
    }
}
