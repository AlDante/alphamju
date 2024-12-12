cmake_minimum_required(VERSION 3.30)
project(alphamju)

set(CMAKE_CXX_STANDARD 17)

#find_package(Boost REQUIRED COMPONENTS regex)
include_directories(../dds/include ../dds/examples ../../dev/catboost/contrib/libs/cxxsupp/openmp . ${Boost_INCLUDE_DIRS} )
add_executable(alphamju
        alphamju.cpp
        /Users/david/Documents/dev/CLionProjects/dds/include/dll.h
        strlcpy.c
        strlcpy.h)
target_link_libraries(alphamju -static)
target_link_libraries(alphamju ${Boost_LIBRARIES})
