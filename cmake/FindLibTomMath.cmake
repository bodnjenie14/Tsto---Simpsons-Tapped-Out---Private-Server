# Find LibTomMath
# Find the native LibTomMath includes and library
#
#  LIBTOMMATH_INCLUDE_DIRS - where to find tommath.h, etc.
#  LIBTOMMATH_LIBRARIES    - List of libraries when using LibTomMath.
#  LIBTOMMATH_FOUND       - True if LibTomMath found.

find_path(LIBTOMMATH_INCLUDE_DIR
    NAMES tommath.h
    PATHS /usr/include /usr/local/include
)

find_library(LIBTOMMATH_LIBRARY
    NAMES tommath libtommath
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibTomMath DEFAULT_MSG
    LIBTOMMATH_LIBRARY LIBTOMMATH_INCLUDE_DIR)

if(LIBTOMMATH_FOUND)
    set(LIBTOMMATH_LIBRARIES ${LIBTOMMATH_LIBRARY})
    set(LIBTOMMATH_INCLUDE_DIRS ${LIBTOMMATH_INCLUDE_DIR})
endif()

mark_as_advanced(LIBTOMMATH_INCLUDE_DIR LIBTOMMATH_LIBRARY) 