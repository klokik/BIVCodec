cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(BIVCodec)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-O4 -pthread)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")


find_package(OpenCV REQUIRED)
find_package ( Threads REQUIRED )

add_executable(basic_test main.cc)
target_link_libraries( basic_test ${CMAKE_THREAD_LIBS_INIT})

add_executable(video_stream video_stream.cc)
target_link_libraries(video_stream ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})

add_executable(video_util video_util.cc)
target_link_libraries(video_util ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})

add_executable(video_diff video_diff.cc)
target_link_libraries(video_diff ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})