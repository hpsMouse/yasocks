cmake_minimum_required(VERSION 2.8)

project(yasocks)

add_definitions(-Wall -Wextra -Weffc++ -std=c++11 -pthread)
set(CMAKE_EXE_LINKER_FLAGS -pthread)

add_executable(yasocks main.cpp handle_client.cpp logging.cpp protocol_types.cpp rules.cpp)

target_link_libraries(yasocks boost_system)
