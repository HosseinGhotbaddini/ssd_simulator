cmake_minimum_required(VERSION 3.16)
project(ssd_simulator)

set(CMAKE_CXX_STANDARD 14)
find_package(Boost REQUIRED COMPONENTS program_options)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
    add_executable(ssd_simulator main.cpp ftl.cpp)
    target_link_libraries(ssd_simulator ${Boost_LIBRARIES})
endif()
