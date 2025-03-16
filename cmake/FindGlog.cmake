# Find Glog - Google logging library
#
# The following variables are set if Glog is found:
#  GLOG_FOUND        - True when Glog is found
#  GLOG_INCLUDE_DIRS - The directory where glog/logging.h is located
#  GLOG_LIBRARIES    - The Glog libraries

find_path(GLOG_INCLUDE_DIR
    NAMES glog/logging.h
    PATHS
        /usr/include
        /usr/local/include
        $ENV{GLOG_ROOT}/include
)

find_library(GLOG_LIBRARY
    NAMES glog
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /usr/local/lib64
        $ENV{GLOG_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glog DEFAULT_MSG GLOG_LIBRARY GLOG_INCLUDE_DIR)

if(GLOG_FOUND)
    set(GLOG_INCLUDE_DIRS ${GLOG_INCLUDE_DIR})
    set(GLOG_LIBRARIES ${GLOG_LIBRARY})

    if(NOT TARGET glog::glog)
        add_library(glog::glog UNKNOWN IMPORTED)
        set_target_properties(glog::glog PROPERTIES
            IMPORTED_LOCATION "${GLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLOG_INCLUDE_DIR}"
        )
    endif()
endif()
