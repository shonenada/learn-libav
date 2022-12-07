cmake_minimum_required(VERSION 3.16)
project(learn_libav)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(LIBAV REQUIRED)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/demo.mp4 DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

set(LIBS ${LIBAV_LIBRARIES})

add_executable(learn_libav src/main.cpp)
target_link_libraries(learn_libav ${LIBS})
