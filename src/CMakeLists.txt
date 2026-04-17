add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance json_aux)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set flag)

add_library(localize localize.cpp)
target_link_libraries(localize preprocess)

add_library(graph graph.cpp)
target_link_libraries(graph flag)

add_library(solver solver.cpp)
target_link_libraries(solver localize graph)
