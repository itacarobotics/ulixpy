cmake_minimum_required(VERSION 3.16)

project(ulixarm_py LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Build against the Pinocchio that ships inside the `pin` wheel (a build and
# runtime dependency, see pyproject.toml), so the C++ extension and Python's
# `pinocchio` module always use the same library. The wheel installs its C++
# headers, CMake configs and shared libraries under site-packages/cmeel.prefix.
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m cmeel cmake
    OUTPUT_VARIABLE CMEEL_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE CMEEL_RESULT
)
if(CMEEL_RESULT EQUAL 0 AND IS_DIRECTORY "${CMEEL_PREFIX}")
    message(STATUS "Using Pinocchio from the pin wheel: ${CMEEL_PREFIX}")
    list(PREPEND CMAKE_PREFIX_PATH "${CMEEL_PREFIX}")
else()
    message(WARNING "cmeel prefix not found; falling back to a system Pinocchio")
endif()

add_subdirectory(cpp/ulixarm_cpp)

pybind11_add_module(_core src/bindings.cpp)

target_link_libraries(_core PRIVATE ulixarm_core)

# At runtime, load Pinocchio's libraries from the pin wheel installed next to
# this package: site-packages/ulixpy/_core.so -> site-packages/cmeel.prefix/lib
if(APPLE)
    set(_ulixpy_origin "@loader_path")
else()
    set(_ulixpy_origin "$ORIGIN")
endif()
set_target_properties(_core PROPERTIES
    INSTALL_RPATH "${_ulixpy_origin}/../cmeel.prefix/lib"
)

install(TARGETS _core DESTINATION ulixpy)
install(FILES cpp/ulixarm_cpp/ulixarm_description/src/ulixarm_description/urdf/robot.urdf DESTINATION ulixpy)
