add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance json_aux)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set flag)

add_library(event_graph event_graph.cpp)
target_link_libraries(event_graph flag)

add_library(solver solver.cpp)
target_include_directories(solver PUBLIC ${GUROBI_INC})
target_link_libraries(solver preprocess event_graph ${GUROBI_API_LIB} ${GUROBI_BASE_LIB})
