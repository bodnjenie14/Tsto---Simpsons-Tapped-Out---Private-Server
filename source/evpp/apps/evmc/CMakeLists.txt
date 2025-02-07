
set(CMAKE_C_FLAGS -std=gnu99)
file(GLOB evmc_SRCS *.cc
                    *.h
                    ${PROJECT_SOURCE_DIR}/3rdparty/libhashkit/*.c)
file(GLOB evmc_PUBLIC_HEADERS *.h)

add_library(evmc_static STATIC ${evmc_SRCS})
target_link_libraries(evmc_static ${LIBRARIES})
target_include_directories(evmc_static INTERFACE $<INSTALL_INTERFACE:include>)

if (UNIX)
    if(BUILD_SHARED_LIBS)
    add_library(evmc SHARED ${evmc_SRCS})
    target_link_libraries(evmc ${LIBRARIES})
    target_include_directories(evmc INTERFACE $<INSTALL_INTERFACE:include>)
    set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
    include (utils)
    include (packages)
    set_target_properties (
        evmc evnsq PROPERTIES
        VERSION     "${PACKAGE_VERSION}"
        SOVERSION   "${PACKAGE_SOVERSION}"
    )

    install (
      TARGETS evmc
      EXPORT  unofficial-evpp-target
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib)
    endif()
endif (UNIX)

install (
  TARGETS evmc_static
  EXPORT  unofficial-evpp-target
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install (FILES ${evmc_PUBLIC_HEADERS} DESTINATION "include/evmc")


if (NOT EVPP_VCPKG_BUILD)
    add_subdirectory(test)
endif ()