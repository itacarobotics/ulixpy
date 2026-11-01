cmake_minimum_required(VERSION 3.16)

project(ulixarm_py LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pybind11 CONFIG REQUIRED)

add_subdirectory(cpp/ulixarm_cpp)

pybind11_add_module(_core src/bindings.cpp)

target_link_libraries(_core PRIVATE ulixarm_core)

install(TARGETS _core DESTINATION ulixpy)
install(FILES cpp/ulixarm_cpp/ulixarm_description/src/ulixarm_description/urdf/robot.urdf DESTINATION ulixpy)
