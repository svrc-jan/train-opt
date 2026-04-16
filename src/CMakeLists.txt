add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance json_aux)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set)

add_library(graph graph.cpp)
target_link_libraries(graph flag)

add_library(path_and_cycle path_and_cycle.cpp)
target_link_libraries(path_and_cycle instance graph)

# add_library(path_select path_select.cpp)
# target_include_directories(path_select PUBLIC ${GUROBI_INC})
# target_link_libraries(path_select preprocess ${GUROBI_API_LIB} ${GUROBI_BASE_LIB})
