cmake_minimum_required(VERSION 3.13)

project(train_opt)

set(CMAKE_CXX_COMPILER g++-14)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/exe)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)


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


add_compile_options(-Wall -Wextra -g)

# optimize stable parts
target_compile_options(instance PRIVATE -O3)
target_compile_options(preprocess PRIVATE -O3)
