cmake_minimum_required(VERSION 3.16)

project(ulixarm_py LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(pybind11 CONFIG REQUIRED)

add_subdirectory(cpp/ulixarm_cpp EXCLUDE_FROM_ALL)

pybind11_add_module(ulixpy src/bindings.cpp)

target_link_libraries(ulixpy PRIVATE ulixarm_core)

install(TARGETS ulixpy DESTINATION .)