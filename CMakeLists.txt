cmake_minimum_required(VERSION 3.16)
project(learn_libav)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

if(UNIX)
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")
endif(UNIX)

include_directories(${CMAKE_SOURCE_DIR}/includes/)
link_directories(${CMAKE_SOURCE_DIR}/libs/)

find_package(LIBAV REQUIRED)

if(LIBAV_FOUND)
#    message("Found LIBAV libraries in ${LIBAV_LIBRARIES}")
    include_directories(${LIBAV_INCLUDE_DIR})
    link_directories(${LIBAV_LIBRARIES})
    set(LIBS ${LIBAV_LIBRARIES})
else()
    message(LIBAV not found)
    set(LIBS )
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/demo.mp4 DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

add_executable(parse_video src/parse_video.cpp)
target_link_libraries(parse_video ${LIBS})

add_executable(decode_encode src/decode_encode.cpp)
target_link_libraries(decode_encode ${LIBS})

add_executable(transcoding src/transcoding.cpp)
target_link_libraries(transcoding ${LIBS})
