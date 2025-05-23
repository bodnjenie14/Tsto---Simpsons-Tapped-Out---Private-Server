cmake_minimum_required (VERSION 2.8)

project (safe-evpp C CXX)

if (UNIX)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()

    set (CXX_FLAGS
         -g
         -fPIC
         -Wall
         -Wno-unused-parameter
         -Wno-unused-function
         -Wunused-variable
         -Wunused-value
         -Wshadow
         -Wcast-qual
         -Wcast-align
         -Wwrite-strings
         -Wsign-compare
         -Winvalid-pch
         -fms-extensions
         -Wfloat-equal
         -Wextra
         -std=c++11
    )
    include_directories (${PROJECT_SOURCE_DIR}
                         ${PROJECT_SOURCE_DIR}/3rdparty)
else (UNIX)
set (CXX_FLAGS -DWIN32 -D_WIN32 -EHsc)
    set(VcpkgRoot ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    include_directories(${VcpkgRoot}/include
                        ${PROJECT_SOURCE_DIR}
                        ${PROJECT_SOURCE_DIR}/3rdparty/wininclude)

    # On windows, Debug mode
    link_directories(${VcpkgRoot}/debug/lib ${VcpkgRoot}/debug/lib/manual-link)

    # On Windows, Release mode
    # link_directories(${VcpkgRoot}/lib ${VcpkgRoot}/lib/manual-link)
endif (UNIX)

string (REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

if (UNIX)
    SET(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -ggdb -D_DEBUG -DGOOGLE_STRIP_LOG=0")
    SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -ggdb -DNDEBUG -DGOOGLE_STRIP_LOG=1")
    find_package(glog CONFIG REQUIRED)
    find_path(GLOG_INCLUDE_DIRS logging.h PATH_SUFFIXES glog)
    find_package(Libevent CONFIG REQUIRED)
    list(APPEND DEPENDENT_INCLUDE_DIRS "${GLOG_INCLUDE_DIRS}/../")
    SET(DEPENDENT_LIBRARIES libevent::core libevent::extra libevent::openssl glog::glog pthread)
else (UNIX)
    find_package(glog CONFIG REQUIRED)
    find_package(Libevent CONFIG REQUIRED)
    SET(DEPENDENT_LIBRARIES libevent::core libevent::extra libevent::openssl glog::glog)
endif (UNIX)

if (CMAKE_BENCHMARK_TESTING)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DH_BENCHMARK_TESTING=1")
endif (CMAKE_BENCHMARK_TESTING)

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

#set (MACOSX_RPATH ${LIBRARY_OUTPUT_PATH})
set (CMAKE_MACOSX_RPATH 0)

message (STATUS "This is BINARY dir " ${PROJECT_BINARY_DIR})
message (STATUS "This is SOURCE dir " ${PROJECT_SOURCE_DIR})
message (STATUS "CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message (STATUS "EXECUTABLE_OUTPUT_PATH " ${EXECUTABLE_OUTPUT_PATH})
message (STATUS "LIBRARY_OUTPUT_PATH " ${LIBRARY_OUTPUT_PATH})

add_subdirectory (evpp)
add_subdirectory (apps)
if (NOT EVPP_VCPKG_BUILD)
    add_subdirectory (test)
    add_subdirectory (examples)
    add_subdirectory (benchmark)
endif ()

set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include (utils)
include (packages)

include (CPack)
include (CTest)

install(EXPORT unofficial-evpp-target
        FILE unofficial-evpp-target.cmake
        NAMESPACE unofficial::evpp::
        DESTINATION share/unofficial-evpp
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/unofficial-evpp-config.cmake"
[[include(CMakeFindDependencyMacro)
find_dependency(glog CONFIG)
find_dependency(Libevent CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/unofficial-evpp-target.cmake")
]])
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/unofficial-evpp-config.cmake DESTINATION "share/unofficial-evpp")