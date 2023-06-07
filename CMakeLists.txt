cmake_minimum_required(VERSION 3.12)

project(thread_pool_test LANGUAGES CXX)

add_executable(thread_pool_test test.cpp thread_pool.h)