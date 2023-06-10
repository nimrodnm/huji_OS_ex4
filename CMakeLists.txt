cmake_minimum_required(VERSION 3.25)
project(huji_OS_ex4)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(huji_OS_ex4
        MemoryConstants.h
        PhysicalMemory.cpp
        PhysicalMemory.h
        SimpleTest.cpp
        VirtualMemory.cpp
        VirtualMemory.h VirtualMemory.cpp)
