include(FindProtobuf)

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
