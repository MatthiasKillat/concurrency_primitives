cmake_minimum_required(VERSION 2.8.3)
project(concurrency_primitives )

include_directories( include )

FILE(GLOB SOURCES "*.cpp")

add_executable(condition_variable
  main.cpp
)

target_link_libraries(condition_variable pthread rt)

add_executable(auto_reset_event
  autoresetevent_main.cpp)

target_link_libraries(auto_reset_event pthread rt)


