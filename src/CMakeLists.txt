add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance files)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set)
