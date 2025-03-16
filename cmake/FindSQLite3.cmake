# Find SQLite3
#
# SQLITE3_INCLUDE_DIRS - where to find sqlite3.h
# SQLITE3_LIBRARIES - List of libraries when using SQLite3
# SQLITE3_FOUND - True if SQLite3 found

find_path(SQLITE3_INCLUDE_DIR
    NAMES sqlite3.h
    PATHS
        /usr/include
        /usr/local/include
)

find_library(SQLITE3_LIBRARY
    NAMES sqlite3
    PATHS
        /usr/lib
        /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3
    REQUIRED_VARS SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR
)

if(SQLITE3_FOUND)
    set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
    set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
endif()
