cmake_minimum_required(VERSION 3.15)
project(tsto-server)

# Set C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add platform-specific definitions
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    set(PLATFORM_WINDOWS ON)
elseif(UNIX)
    set(PLATFORM_LINUX ON)
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Dependencies
include(FetchContent)

# RapidJSON
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG master
)
FetchContent_MakeAvailable(rapidjson)

# Add required libraries
find_package(LibTomCrypt REQUIRED)
find_package(LibTomMath REQUIRED)
find_package(Libevent REQUIRED)
find_package(ProtobufWithOptions REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(glog REQUIRED)
find_package(ZLIB REQUIRED)

# Platform-specific dependencies
if(WIN32)
    # Windows uses minizip
    find_package(minizip REQUIRED)
else()
    find_package(LibZip REQUIRED)
endif()

# Optional Discord Game SDK
find_package(DiscordGameSDK)
if(DISCORD_GAME_SDK_FOUND)
    add_definitions(-DDISCORD_SDK_ENABLED)
endif()

# Custom function to generate protobuf files with optional fields support
function(PROTOBUF_GENERATE_WITH_OPTIONS SRCS HDRS)
    if(NOT ARGN)
        message(SEND_ERROR "Error: PROTOBUF_GENERATE_WITH_OPTIONS() called without any proto files")
        return()
    endif()

    set(${SRCS})
    set(${HDRS})

    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)

        list(APPEND ${SRCS} "${CMAKE_BINARY_DIR}/generated/${FIL_WE}.pb.cc")
        list(APPEND ${HDRS} "${CMAKE_BINARY_DIR}/generated/${FIL_WE}.pb.h")

        add_custom_command(
            OUTPUT "${CMAKE_BINARY_DIR}/generated/${FIL_WE}.pb.cc"
                   "${CMAKE_BINARY_DIR}/generated/${FIL_WE}.pb.h"
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            ARGS --experimental_allow_proto3_optional
                 --cpp_out ${CMAKE_BINARY_DIR}/generated
                 -I ${CMAKE_CURRENT_SOURCE_DIR}/source/server/protobufs
                 ${ABS_FIL}
            DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE}
            COMMENT "Running C++ protocol buffer compiler on ${FIL} with proto3 optional support"
            VERBATIM
        )
    endforeach()

    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

# Generate Protocol Buffers
file(GLOB PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/server/protobufs/*.proto"
)

# Generate protobuf files in the build directory
set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# Generate C++ files from proto files with proto3 optional support
PROTOBUF_GENERATE_WITH_OPTIONS(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create symbolic links for easier include paths
if(UNIX)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${PROTO_GEN_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/source/server/build
    )
else()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROTO_GEN_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/source/server/build
    )
endif()

# Add the generated directory to include paths
include_directories(
    ${PROTO_GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source/server/build
)

# EVPP Library
add_library(evpp STATIC
    source/evpp/evpp/buffer.cc
    source/evpp/evpp/connector.cc
    source/evpp/evpp/dns_resolver.cc
    source/evpp/evpp/event_loop.cc
    source/evpp/evpp/event_loop_thread.cc
    source/evpp/evpp/event_loop_thread_pool.cc
    source/evpp/evpp/event_watcher.cc
    source/evpp/evpp/duration.cc
    source/evpp/evpp/fd_channel.cc
    source/evpp/evpp/inner_pre.cc
    source/evpp/evpp/invoke_timer.cc
    source/evpp/evpp/libevent.cc
    source/evpp/evpp/listener.cc
    source/evpp/evpp/sockets.cc
    source/evpp/evpp/tcp_client.cc
    source/evpp/evpp/tcp_conn.cc
    source/evpp/evpp/tcp_server.cc
    source/evpp/evpp/udp/sync_udp_client.cc
    source/evpp/evpp/udp/udp_server.cc
    source/evpp/evpp/http/context.cc
    source/evpp/evpp/http/http_server.cc
    source/evpp/evpp/http/service.cc
    source/evpp/evpp/httpc/conn.cc
    source/evpp/evpp/httpc/conn_pool.cc
    source/evpp/evpp/httpc/request.cc
    source/evpp/evpp/httpc/response.cc
    source/evpp/evpp/httpc/url_parser.cc
)

target_include_directories(evpp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/evpp
    ${LIBEVENT_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(evpp PUBLIC 
    ${LIBEVENT_LIBRARIES}
    glog::glog
)

# Utilities Library
file(GLOB_RECURSE UTILITIES_SOURCES
    "source/utilities/*.cpp"
    "source/utilities/*.hpp"
)

add_library(utilities STATIC ${UTILITIES_SOURCES})

target_include_directories(utilities PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/server
    ${rapidjson_SOURCE_DIR}/include
    ${SQLITE3_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${LIBTOMCRYPT_INCLUDE_DIRS}
    $<$<BOOL:${DISCORD_GAME_SDK_FOUND}>:${DISCORD_GAME_SDK_INCLUDE_DIRS}>
)

target_link_libraries(utilities PUBLIC
    ${ZLIB_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    ${LIBTOMCRYPT_LIBRARIES}
    ${LIBTOMMATH_LIBRARIES}
    $<$<BOOL:${DISCORD_GAME_SDK_FOUND}>:${DISCORD_GAME_SDK_LIBRARIES}>
)

# Add platform-specific compile definitions
if(WIN32)
    target_compile_definitions(utilities PRIVATE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DPLATFORM_WINDOWS)
    target_link_libraries(utilities PUBLIC minizip::minizip)
else()
    target_compile_definitions(utilities PRIVATE -DPLATFORM_LINUX)
    target_link_libraries(utilities PUBLIC ${LIBZIP_LIBRARIES})
endif()

# Server Library
file(GLOB_RECURSE SERVER_LIB_SOURCES
    "source/server/*.cpp"
    "source/server/*.hpp"
    "source/server/tsto/*.cpp"
    "source/server/tsto/*.hpp"
    "source/server/tsto/dashboard/*.cpp"
    "source/server/tsto/dashboard/*.hpp"
	"source/server/tsto/auth/*.cpp"
    "source/server/tsto/auth/*.hpp"
    "source/server/tsto/database/*.cpp"
    "source/server/tsto/database/*.hpp"
	"source/server/tsto/device/*.cpp"
    "source/server/tsto/device/*.hpp"
	"source/server/tsto/events/*.cpp"
    "source/server/tsto/events/*.hpp"
	"source/server/tsto/land/*.cpp"
    "source/server/tsto/land/*.hpp"
	"source/server/tsto/tracking/*.cpp"
    "source/server/tsto/tracking/*.hpp"
	"source/server/tsto/user/*.cpp"
    "source/server/tsto/user/*.hpp"
    "source/server/networking/*.cpp"
    "source/server/networking/*.hpp"
    "source/server/discord/*.cpp"
    "source/server/discord/*.hpp"
    "source/server/debugging/*.cpp"
    "source/server/debugging/*.hpp"
    "source/server/dispatcher/*.cpp"
    "source/server/dispatcher/*.hpp"
    "source/server/file_server/*.cpp"
    "source/server/file_server/*.hpp"
    "source/server/headers/*.cpp"
    "source/server/headers/*.hpp"
    "source/server/updater/*.cpp"
    "source/server/updater/*.hpp"
)

add_library(server_lib STATIC ${SERVER_LIB_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/server
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBEVENT_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(server_lib PUBLIC
    evpp
    utilities
    ${PROTOBUF_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    glog::glog
    $<$<BOOL:${DISCORD_GAME_SDK_FOUND}>:${DISCORD_GAME_SDK_LIBRARIES}>
)

# Main executable
add_executable(tsto-server
    source/server/main.cpp
)

target_include_directories(tsto-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/server
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(tsto-server PRIVATE
    server_lib
    evpp
    utilities
    ${PROTOBUF_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    glog::glog
    $<$<BOOL:${DISCORD_GAME_SDK_FOUND}>:${DISCORD_GAME_SDK_LIBRARIES}>
    $<$<NOT:$<PLATFORM_ID:Windows>>:${LIBTOMCRYPT_LIBRARIES}>
    $<$<NOT:$<PLATFORM_ID:Windows>>:${LIBTOMMATH_LIBRARIES}>
    $<$<NOT:$<PLATFORM_ID:Windows>>:z>
    $<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
    $<$<NOT:$<PLATFORM_ID:Windows>>:dl>
)

# Add compiler flags
if(UNIX)
    add_compile_options(-Wall -Wextra)
endif()

# Installation
install(TARGETS tsto-server
    RUNTIME DESTINATION bin
)