#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#else
// Windows type definitions for Linux
typedef void* HANDLE;
typedef void* HMODULE;
typedef unsigned long DWORD;
typedef void* LPVOID;
typedef const char* LPCSTR;
typedef unsigned char BYTE;
typedef unsigned short WORD;

struct _IMAGE_DOS_HEADER {
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    DWORD e_lfanew;
};

struct _IMAGE_FILE_HEADER {
    WORD Machine;
    WORD NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD SizeOfOptionalHeader;
    WORD Characteristics;
};

struct _IMAGE_OPTIONAL_HEADER {
    WORD Magic;
    BYTE MajorLinkerVersion;
    BYTE MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;
    DWORD BaseOfCode;
    DWORD BaseOfData;
    DWORD ImageBase;
    DWORD SectionAlignment;
    DWORD FileAlignment;
    WORD MajorOperatingSystemVersion;
    WORD MinorOperatingSystemVersion;
    WORD MajorImageVersion;
    WORD MinorImageVersion;
    WORD MajorSubsystemVersion;
    WORD MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;
    DWORD SizeOfHeaders;
    DWORD CheckSum;
    WORD Subsystem;
    WORD DllCharacteristics;
    DWORD SizeOfStackReserve;
    DWORD SizeOfStackCommit;
    DWORD SizeOfHeapReserve;
    DWORD SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;
};

struct _IMAGE_NT_HEADERS {
    DWORD Signature;
    _IMAGE_FILE_HEADER FileHeader;
    _IMAGE_OPTIONAL_HEADER OptionalHeader;
};

struct _IMAGE_SECTION_HEADER {
    BYTE Name[8];
    union {
        DWORD PhysicalAddress;
        DWORD VirtualSize;
    } Misc;
    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD NumberOfRelocations;
    WORD NumberOfLinenumbers;
    DWORD Characteristics;
};

typedef struct _IMAGE_DOS_HEADER* PIMAGE_DOS_HEADER;
typedef struct _IMAGE_NT_HEADERS* PIMAGE_NT_HEADERS;
typedef struct _IMAGE_OPTIONAL_HEADER* PIMAGE_OPTIONAL_HEADER;
typedef struct _IMAGE_SECTION_HEADER* PIMAGE_SECTION_HEADER;

#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define GetProcAddress(handle, name) nullptr
#define FreeLibrary(handle) (void)handle
#define IMAGE_FIRST_SECTION(ntheader) ((PIMAGE_SECTION_HEADER)((BYTE*)ntheader + sizeof(_IMAGE_NT_HEADERS)))
#endif

namespace utils::nt
{
    class library final
    {
    public:
        library() = default;
        explicit library(HMODULE handle);
        library(const library& a) : module_(a.module_) {}
        library(library&& a) noexcept : module_(a.module_)
        {
            a.module_ = nullptr;
        }

        ~library()
        {
            if (this->module_)
            {
                FreeLibrary(this->module_);
            }
        }

        static library load(const char* name);
        static library load(const std::string& name);
        static library get_by_address(const void* address);

        operator HMODULE() const { return module_; }
        operator bool() const { return is_valid(); }
        bool operator==(const library& obj) const { return module_ == obj.module_; }

        bool is_valid() const { return module_ != nullptr; }
        std::string get_name() const;
        std::string get_path() const;
        void free();

        HMODULE get_handle() const { return module_; }
        PIMAGE_NT_HEADERS get_nt_headers() const;
        PIMAGE_DOS_HEADER get_dos_header() const;
        PIMAGE_OPTIONAL_HEADER get_optional_header() const;
        std::vector<PIMAGE_SECTION_HEADER> get_section_headers() const;
        std::uint8_t* get_ptr() const;

        template <typename T>
        T get_proc(const std::string& process) const
        {
#ifdef _WIN32
            return reinterpret_cast<T>(GetProcAddress(this->module_, process.data()));
#else
            return nullptr;
#endif
        }

    private:
        HMODULE module_{nullptr};
    };

    template <HANDLE InvalidHandle = nullptr>
    class handle final
    {
    public:
        handle() = default;
        handle(const HANDLE h) : handle_(h) {}
        handle(const handle& h) : handle_(h.handle_) {}
        handle(handle&& h) noexcept : handle_(h.handle_)
        {
            h.handle_ = InvalidHandle;
        }

        ~handle()
        {
            if (this->handle_ != InvalidHandle)
            {
#ifdef _WIN32
                CloseHandle(this->handle_);
#endif
            }
        }

        handle& operator=(handle&& h) noexcept
        {
            if (this != &h)
            {
                this->~handle();
                this->handle_ = h.handle_;
                h.handle_ = InvalidHandle;
            }
            return *this;
        }

        handle& operator=(HANDLE h) noexcept
        {
            if (this->handle_ != h)
            {
                this->~handle();
                this->handle_ = h;
            }
            return *this;
        }

        operator HANDLE() const { return handle_; }

    private:
        HANDLE handle_{InvalidHandle};
    };
}
