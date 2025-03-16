# Find Discord Game SDK
#
# DISCORD_GAME_SDK_INCLUDE_DIRS - where to find discord_game_sdk.h
# DISCORD_GAME_SDK_LIBRARIES - List of libraries when using Discord Game SDK
# DISCORD_GAME_SDK_FOUND - True if Discord Game SDK found

find_path(DISCORD_GAME_SDK_INCLUDE_DIR
    NAMES discord_game_sdk.h
    PATHS
        ${CMAKE_SOURCE_DIR}/third_party/discord_game_sdk/include
        /usr/include
        /usr/local/include
)

find_library(DISCORD_GAME_SDK_LIBRARY
    NAMES discord_game_sdk
    PATHS
        ${CMAKE_SOURCE_DIR}/third_party/discord_game_sdk/lib
        /usr/lib
        /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DiscordGameSDK
    REQUIRED_VARS DISCORD_GAME_SDK_LIBRARY DISCORD_GAME_SDK_INCLUDE_DIR
)

if(DISCORD_GAME_SDK_FOUND)
    set(DISCORD_GAME_SDK_LIBRARIES ${DISCORD_GAME_SDK_LIBRARY})
    set(DISCORD_GAME_SDK_INCLUDE_DIRS ${DISCORD_GAME_SDK_INCLUDE_DIR})
endif()
