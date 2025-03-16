# Find LibZip
#
# LIBZIP_INCLUDE_DIR - Where to find zip.h
# LIBZIP_LIBRARIES - List of libraries when using LibZip
# LIBZIP_FOUND - True if LibZip found

find_path(LIBZIP_INCLUDE_DIR
    NAMES zip.h
    PATHS
        ${LIBZIP_ROOT}/include
        /usr/include
        /usr/local/include
)

find_library(LIBZIP_LIBRARY
    NAMES zip libzip
    PATHS
        ${LIBZIP_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibZip
    REQUIRED_VARS
        LIBZIP_LIBRARY
        LIBZIP_INCLUDE_DIR
)

if(LIBZIP_FOUND)
    set(LIBZIP_LIBRARIES ${LIBZIP_LIBRARY})
    set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})
endif()

mark_as_advanced(
    LIBZIP_INCLUDE_DIR
    LIBZIP_LIBRARY
) 