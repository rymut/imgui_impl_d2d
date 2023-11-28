cmake_minimum_required(VERSION 3.8)
project(test_package LANGUAGES CXX)

find_package(imgui_impl_2d2 REQUIRED)

add_executable(${PROJECT_NAME} test_package.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui_impl_d2d::imgui_impl_d2d)
