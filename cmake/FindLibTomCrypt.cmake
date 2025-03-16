# Find LibTomCrypt
#
# LIBTOMCRYPT_INCLUDE_DIR - Where to find tomcrypt.h
# LIBTOMCRYPT_LIBRARIES - List of libraries when using LibTomCrypt
# LIBTOMCRYPT_FOUND - True if LibTomCrypt found

find_path(LIBTOMCRYPT_INCLUDE_DIR
    NAMES tomcrypt.h
    PATHS
        ${LIBTOMCRYPT_ROOT}/include
        /usr/include
        /usr/local/include
)

find_library(LIBTOMCRYPT_LIBRARY
    NAMES tomcrypt libtomcrypt
    PATHS
        ${LIBTOMCRYPT_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibTomCrypt
    REQUIRED_VARS
        LIBTOMCRYPT_LIBRARY
        LIBTOMCRYPT_INCLUDE_DIR
)

if(LIBTOMCRYPT_FOUND)
    set(LIBTOMCRYPT_LIBRARIES ${LIBTOMCRYPT_LIBRARY})
    set(LIBTOMCRYPT_INCLUDE_DIRS ${LIBTOMCRYPT_INCLUDE_DIR})
endif()

mark_as_advanced(
    LIBTOMCRYPT_INCLUDE_DIR
    LIBTOMCRYPT_LIBRARY
) 