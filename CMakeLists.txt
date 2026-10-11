cmake_minimum_required(VERSION 3.16)
project(armv8_instruction_simulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(armv8-sim src/arm2.c)
