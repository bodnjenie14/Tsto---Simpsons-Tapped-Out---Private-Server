# Find Libevent
#
# LIBEVENT_INCLUDE_DIRS - Where to find event2/event.h
# LIBEVENT_LIBRARIES - List of libraries when using Libevent
# LIBEVENT_FOUND - True if Libevent found

find_path(LIBEVENT_INCLUDE_DIR
    NAMES event2/event.h
    PATHS
        ${LIBEVENT_ROOT}/include
        /usr/include
        /usr/local/include
)

find_library(LIBEVENT_LIBRARY
    NAMES event libevent
    PATHS
        ${LIBEVENT_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

find_library(LIBEVENT_CORE_LIBRARY
    NAMES event_core libevent_core
    PATHS
        ${LIBEVENT_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libevent
    REQUIRED_VARS
        LIBEVENT_LIBRARY
        LIBEVENT_CORE_LIBRARY
        LIBEVENT_INCLUDE_DIR
)

if(LIBEVENT_FOUND)
    set(LIBEVENT_LIBRARIES ${LIBEVENT_LIBRARY} ${LIBEVENT_CORE_LIBRARY})
    set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
endif()

mark_as_advanced(
    LIBEVENT_INCLUDE_DIR
    LIBEVENT_LIBRARY
    LIBEVENT_CORE_LIBRARY
) 