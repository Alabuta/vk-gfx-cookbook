cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

set(EXECUTABLE_TARGET_NAME chapter01_taskflow)
add_executable(${EXECUTABLE_TARGET_NAME} ./src/main.cxx)

configure_cookbook_target(${EXECUTABLE_TARGET_NAME})
