cmake_minimum_required(VERSION 3.5)
project(concurrency_primitives )

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("-O3")

include_directories( include )

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

add_executable(test_waitset
  test_waitset.cpp)

target_link_libraries(test_waitset pthread rt)

add_executable(test_waitset_pub_sub
  test_waitset_pub_sub.cpp)

target_link_libraries(test_waitset_pub_sub pthread rt)


