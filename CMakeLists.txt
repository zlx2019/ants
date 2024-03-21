cmake_minimum_required(VERSION 3.27)
project(thread_pool CXX)

set(CMAKE_CXX_STANDARD 11)

add_executable(thread_pool main.cpp ThreadPool.cpp TaskQueue.cpp Task.cpp)
