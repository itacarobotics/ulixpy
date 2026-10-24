cmake_minimum_required(VERSION 3.16)

project(ulixarm_py LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Eigen and Pinocchio come from the pin wheel
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m cmeel cmake
    OUTPUT_VARIABLE CMEEL_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
list(APPEND CMAKE_PREFIX_PATH "${CMEEL_PREFIX}")

add_subdirectory(cpp/ulixarm_cpp EXCLUDE_FROM_ALL)

# libserialport is linked statically on macOS, so add its frameworks
if(APPLE)
    target_link_libraries(ulixarm_core PUBLIC "-framework IOKit" "-framework CoreFoundation")
endif()

pybind11_add_module(_core src/bindings.cpp)

target_link_libraries(_core PRIVATE ulixarm_core)

# load pin's shared libraries from the installed pin wheel
if(APPLE)
    set_target_properties(_core PROPERTIES INSTALL_RPATH "@loader_path/../cmeel.prefix/lib")
else()
    set_target_properties(_core PROPERTIES INSTALL_RPATH "$ORIGIN/../cmeel.prefix/lib")
endif()

install(TARGETS _core DESTINATION ulixpy)
install(FILES cpp/ulixarm_cpp/ulixarm_description/src/ulixarm_description/urdf/robot.urdf DESTINATION ulixpy)
