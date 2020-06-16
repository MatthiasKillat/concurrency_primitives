cmake_minimum_required(VERSION 2.8.3)
project(concurrency_primitives )

add_compile_options("-O3")

include_directories( include )

FILE(GLOB SOURCES "*.cpp")

add_executable(condition_variable
  main.cpp
)

target_link_libraries(condition_variable pthread rt)

add_executable(auto_reset_event
  autoresetevent_main.cpp)

target_link_libraries(auto_reset_event pthread rt)

add_executable(test_locks
  test_locks.cpp)

target_link_libraries(test_locks pthread rt)

add_executable(test_semaphores
  test_semaphores.cpp)

target_link_libraries(test_semaphores pthread rt)


