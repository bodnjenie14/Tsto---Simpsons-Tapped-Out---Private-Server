
include_directories(${PROJECT_SOURCE_DIR}/apps ${PROJECT_SOURCE_DIR}/3rdparty)

if (UNIX)
set(LIBRARIES evpp_concurrentqueue libevent::core libevent::extra libevent::openssl glog::glog pthread)
link_directories("/home/s/safe/lib" ${PROJECT_BUILD_DIR}/lib)
else(UNIX)
set(LIBRARIES evpp_static event glog)
link_directories(${PROJECT_SOURCE_DIR}/vsprojects/bin/${CMAKE_BUILD_TYPE}/
				 ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE}/)
endif(UNIX)

add_subdirectory(evnsq)
add_subdirectory(evmc)
