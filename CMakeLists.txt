cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(BIVCodec)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-O4)

add_executable(basic_test main.cc)
# target_link_libraries( basic_test )

find_package(OpenCV REQUIRED)

add_executable(video_stream video_stream.cc)
target_link_libraries(video_stream ${OpenCV_LIBS})
