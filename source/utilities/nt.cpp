#include "nt.hpp"
#include <array>

#ifdef _WIN32
#include <psapi.h>
#endif

namespace utils::nt
{
    library::library(HMODULE handle) : module_(handle) {}

    library library::load(const char* name)
    {
#ifdef _WIN32
        return library(LoadLibraryA(name));
#else
        return library(nullptr);
#endif
    }

    library library::load(const std::string& name)
    {
        return load(name.c_str());
    }

    library library::get_by_address(const void* address)
    {
#ifdef _WIN32
        HMODULE handle = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCSTR>(address), &handle);
        return library(handle);
#else
        return library(nullptr);
#endif
    }

    void library::free()
    {
        if (module_)
        {
            FreeLibrary(module_);
            module_ = nullptr;
        }
    }

    std::string library::get_name() const
    {
#ifdef _WIN32
        std::array<char, MAX_PATH> name{};
        if (!module_ || !GetModuleBaseNameA(GetCurrentProcess(), module_, name.data(),
            static_cast<DWORD>(name.size())))
        {
            return {};
        }
        return name.data();
#else
        return {};
#endif
    }

    std::string library::get_path() const
    {
#ifdef _WIN32
        std::array<char, MAX_PATH> name{};
        if (!module_ || !GetModuleFileNameExA(GetCurrentProcess(), module_, name.data(),
            static_cast<DWORD>(name.size())))
        {
            return {};
        }
        return name.data();
#else
        return {};
#endif
    }

    PIMAGE_DOS_HEADER library::get_dos_header() const
    {
        if (!is_valid()) return nullptr;
        return reinterpret_cast<PIMAGE_DOS_HEADER>(get_ptr());
    }

    PIMAGE_NT_HEADERS library::get_nt_headers() const
    {
        if (!is_valid()) return nullptr;
        const auto dos_header = get_dos_header();
        if (!dos_header) return nullptr;
        return reinterpret_cast<PIMAGE_NT_HEADERS>(get_ptr() + dos_header->e_lfanew);
    }

    PIMAGE_OPTIONAL_HEADER library::get_optional_header() const
    {
        if (!is_valid()) return nullptr;
        const auto nt_headers = get_nt_headers();
        if (!nt_headers) return nullptr;
        return &nt_headers->OptionalHeader;
    }

    std::vector<PIMAGE_SECTION_HEADER> library::get_section_headers() const
    {
        std::vector<PIMAGE_SECTION_HEADER> sections;
        
        const auto nt_headers = get_nt_headers();
        if (!nt_headers) return sections;

        auto section = IMAGE_FIRST_SECTION(nt_headers);
        for (WORD i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i, ++section)
        {
            sections.push_back(section);
        }

        return sections;
    }

    std::uint8_t* library::get_ptr() const
    {
        return reinterpret_cast<std::uint8_t*>(module_);
    }
}
