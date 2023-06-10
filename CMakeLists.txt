cmake_minimum_required(VERSION 3.25)
project(huji_OS_ex4)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)
include_directories(yaaraTests)

add_executable(huji_OS_ex4
        MemoryConstants.h
#        yaaraTests/YaaraConstants.h
        PhysicalMemory.cpp
        PhysicalMemory.h
        SimpleTest.cpp
#        yaaraTests/YaaraTest.cpp
        VirtualMemory.cpp
        VirtualMemory.h)
