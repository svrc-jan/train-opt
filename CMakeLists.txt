cmake_minimum_required(VERSION 3.13)

project(train_opt)

set(CMAKE_CXX_COMPILER g++-14)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)

include_directories(
	/usr/include
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/lib
)

find_library(CLP_LIB Clp)
find_library(CLP_SOLVER_LIB ClpSolver)
find_library(COIN_UTILS_LIB CoinUtils)

message(STATUS "coin - libs: " ${CLP_LIB} ", " ${CLP_SOLVER_LIB} ", " ${COIN_UTILS_LIB})

set(GUROBI_API_LIB $ENV{GUROBI_HOME}/lib/libgurobi_c++.a)
set(GUROBI_BASE_LIB $ENV{GUROBI_HOME}/lib/libgurobi120.so)
set(GUROBI_INC $ENV{GUROBI_HOME}/include)

message(STATUS "gurobi - api lib: " ${GUROBI_API_LIB} 
	", base lib: " ${GUROBI_BASE_LIB} 
	", include: " ${GUROBI_INC})

add_subdirectory(src)

target_link_libraries(lin_prog ${CLP_LIB} ${CLP_SOLVER_LIB} ${COIN_UTILS_LIB})

add_executable(instance_test apps/instance_test.cpp)
target_link_libraries(instance_test instance)

add_executable(preprocess_test apps/preprocess_test.cpp)
target_link_libraries(preprocess_test preprocess)

add_executable(path_selector_test apps/path_selector_test.cpp)
target_link_libraries(path_selector_test path_selector)

add_executable(graph_test apps/graph_test.cpp)
target_link_libraries(graph_test graph path_selector rand_int)

add_executable(schedule_test apps/schedule_test.cpp)
target_link_libraries(schedule_test schedule path_selector)

add_executable(solver_test apps/solver_test.cpp)
target_link_libraries(solver_test solver path_selector)

add_executable(lin_prog_test apps/lin_prog_test.cpp)
target_link_libraries(lin_prog_test lin_prog path_selector)

add_executable(gurobi_test apps/gurobi_test.cpp)
target_link_libraries(gurobi_test gurobi_model path_selector)

add_compile_options(-Wall -Wextra -g -DNO_VLA)

# optimize stable parts
target_compile_options(instance PRIVATE -O3)
target_compile_options(preprocess PRIVATE -O3)
target_compile_options(graph PRIVATE -O3)
target_compile_options(schedule PRIVATE -O3)
target_compile_options(solver PRIVATE -O3)


# target_compile_definitions(solver PUBLIC STATIC_LOG_LEVEL=100)
# target_compile_definitions(lin_prog PUBLIC STATIC_LOG_LEVEL=100)
# target_compile_definitions(gurobi_model PUBLIC STATIC_LOG_LEVEL=100)
