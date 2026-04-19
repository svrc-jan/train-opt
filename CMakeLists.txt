cmake_minimum_required(VERSION 3.13)

project(train_opt)

set(CMAKE_CXX_COMPILER g++-14)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CONFIGURATION_TYPES Debug Release Profiling)
message(STATUS "config types: " ${CMAKE_CONFIGURATION_TYPES})

set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -save-temps")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_PROFILING "-g -pg -O3")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/build/debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/build/debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/build/debug/lib)
set(CMAKE_CXX_OUTPUT_E)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_Profiling ${PROJECT_SOURCE_DIR}/build/profiling)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_PROFILING ${PROJECT_SOURCE_DIR}/build/profiling/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_PROFILING ${PROJECT_SOURCE_DIR}/build/profiling/lib)


include_directories(
	/usr/include
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/lib
)

set(GUROBI_API_LIB $ENV{GUROBI_HOME}/lib/libgurobi_c++.a)
set(GUROBI_BASE_LIB $ENV{GUROBI_HOME}/lib/libgurobi120.so)
set(GUROBI_INC $ENV{GUROBI_HOME}/include)

message(STATUS "gurobi - api lib: " ${GUROBI_API_LIB} 
	", base lib: " ${GUROBI_BASE_LIB} 
	", include: " ${GUROBI_INC})

add_subdirectory(src)

add_executable(instance_test apps/instance_test.cpp)
target_link_libraries(instance_test instance)

add_executable(preprocess_test apps/preprocess_test.cpp)
target_link_libraries(preprocess_test preprocess)

add_executable(graph_test apps/graph_test.cpp)
target_link_libraries(graph_test event_graph)

add_executable(solver_test apps/solver_test.cpp)
target_link_libraries(solver_test solver)

add_executable(flag_test apps/flag_test.cpp)
target_link_libraries(flag_test flag)
