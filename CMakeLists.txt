cmake_minimum_required(VERSION 3.26)

project(solver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_subdirectory(glniman)


add_executable(solver solve.cpp)
target_link_libraries(solver gl)

add_executable(text-solver solve-stripped.cpp)


install(TARGETS solver text-solver DESTINATION bin)

