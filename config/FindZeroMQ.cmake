# Find ZeroMQ Headers/Libs

# Variables
# ZMQ_ROOT - set this to a location where ZeroMQ may be found
#
# ZMQ_FOUND - True of ZeroMQ found
# ZMQ_INCLUDE_DIRS - Location of ZeroMQ includes
# ZMQ_LIBRARIS - ZeroMQ libraries

include(FindPackageHandleStandardArgs)

if (NOT ZMQ_ROOT)
    set(ZMQ_ROOT "$ENV{ZMQ_ROOT}")
endif()

if (NOT ZMQ_ROOT)
    find_path(_ZeroMQ_ROOT NAMES include/zmq.h)
else()
    set(_ZeroMQ_ROOT "${ZMQ_ROOT}")
endif()

find_path(ZeroMQ_INCLUDE_DIRS NAMES zmq.h HINTS ${_ZeroMQ_ROOT}/include)

set(_ZeroMQ_H ${ZeroMQ_INCLUDE_DIRS}/zmq.h)

if (NOT ${CMAKE_CXX_PLATFORM_ID} STREQUAL "Windows")
    find_library(ZeroMQ_LIBRARIES NAMES zmq HINTS ${_ZeroMQ_ROOT}/lib)
else()
    find_library(ZeroMQ_LIBRARY_RELEASE NAMES libzmq HINTS ${_ZeroMQ_ROOT}/lib)
    find_library(ZeroMQ_LIBRARY_DEBUG NAMES libzmq_d HINTS ${_ZeroMQ_ROOT}/lib)
    if (ZeroMQ_LIBRARY_DEBUG)
        set(ZeroMQ_LIBRARIES optimized "${ZeroMQ_LIBRARY_RELEASE}" debug "${ZeroMQ_LIBRARY_DEBUG}")
    else()
        set(ZeroMQ_LIBRARIES "${ZeroMQ_LIBRARY_RELEASE}")
    endif()
endif()

# TODO: implement version extracting for Windows
if (NOT ${CMAKE_CXX_PLATFORM_ID} STREQUAL "Windows")
    function(_zmqver_EXTRACT _ZeroMQ_VER_COMPONENT _ZeroMQ_VER_OUTPUT)
        execute_process(
            COMMAND grep "#define ${_ZMQ_VER_COMPONENT}"
            COMMAND cut -d\  -f3
            RESULT_VARIABLE _zmqver_RESULT
            OUTPUT_VARIABLE _zmqver_OUTPUT
            INPUT_FILE ${_ZeroMQ_H}
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(${_ZeroMQ_VER_OUTPUT} ${_zmqver_OUTPUT} PARENT_SCOPE)
    endfunction()

    _zmqver_EXTRACT("ZMQ_VERSION_MAJOR" ZeroMQ_VERSION_MAJOR)
    _zmqver_EXTRACT("ZMQ_VERSION_MINOR" ZeroMQ_VERSION_MINOR)

    set(ZeroMQ_FIND_VERSION_EXACT "${ZMQ_VERSION_MAJOR}.${ZMQ_VERSION_MINOR}")
endif()

find_package_handle_standard_args(ZeroMQ FOUND_VAR ZeroMQ_FOUND
                                      REQUIRED_VARS ZeroMQ_INCLUDE_DIRS ZeroMQ_LIBRARIES
                                      VERSION_VAR ZeroMQ_VERSION)

if (ZeroMQ_FOUND)
    mark_as_advanced(ZeroMQ_FIND_VERSION_EXACT ZeroMQ_VERSION ZeroMQ_INCLUDE_DIRS ZeroMQ_LIBRARIES)
endif()
