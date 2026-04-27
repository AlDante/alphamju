cmake_minimum_required(VERSION 3.30)
project(alphamju)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/opt/local/share/cmake-3.29/Modules/")

message(STATUS,  "CMAKE_MODULE_PATH is: " ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 17)
#SET(BOOST_ROOT "/opt/local/libexec/boost/1.76/")

#SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "C:/win32libs/boost")
#SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "C:/win32libs/boost/lib")

find_package(Boost REQUIRED)
#find_package(OpenMP REQUIRED)

include_directories(../dds/include ../dds/examples /opt/homebrew/opt/libomp/include  . ${Boost_INCLUDE_DIRS} )
#target_link_libraries(dds ${Boost_LIBRARIES})
add_executable(alphamju
        alphamju.cpp
        strlcpy.c
        strlcpy.h)
target_link_libraries(alphamju ${Boost_LIBRARIES}
        /usr/local/lib/libMacDDS.dylib
)
