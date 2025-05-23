file(GLOB evnsq_SRCS *.cc *.h)
file(GLOB evnsq_PUBLIC_HEADERS *.h)

add_library(evnsq_static STATIC ${evnsq_SRCS})
target_link_libraries(evnsq_static ${LIBRARIES})
target_include_directories(evnsq_static INTERFACE $<INSTALL_INTERFACE:include>)

if (UNIX)
    if(BUILD_SHARED_LIBS)
    add_library(evnsq SHARED ${evnsq_SRCS})
    target_link_libraries(evnsq ${LIBRARIES})
    target_include_directories(evnsq INTERFACE $<INSTALL_INTERFACE:include>)
    set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
    include (utils)
    include (packages)
    set_target_properties (
        evnsq PROPERTIES
        VERSION     "${PACKAGE_VERSION}"
        SOVERSION   "${PACKAGE_SOVERSION}"
    )

    install (
      TARGETS evnsq
      EXPORT  unofficial-evpp-target
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib)
    endif()
endif (UNIX)

install (
  TARGETS evnsq_static
  EXPORT  unofficial-evpp-target
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install (FILES ${evnsq_PUBLIC_HEADERS} DESTINATION "include/evnsq")

if (NOT EVPP_VCPKG_BUILD)
    add_subdirectory(evnsqtail)
    add_subdirectory(test)
endif ()
