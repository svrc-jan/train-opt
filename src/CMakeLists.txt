add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance json_aux)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set flag)

add_library(link_graph link_graph.cpp)
target_link_libraries(link_graph preprocess)

add_library(event_graph event_graph.cpp)
target_link_libraries(event_graph flag)

add_library(chunk_manager chunk_manager.cpp)
target_link_libraries(chunk_manager preprocess)

add_library(route_planner route_planner.cpp)
target_include_directories(route_planner PUBLIC ${GUROBI_INC})
target_link_libraries(route_planner chunk_manager link_graph 
	${GUROBI_API_LIB} ${GUROBI_BASE_LIB})

add_library(conflict_resolver conflict_resolver.cpp)
target_include_directories(conflict_resolver PUBLIC ${GUROBI_INC})
target_link_libraries(conflict_resolver chunk_manager link_graph event_graph 
	${GUROBI_API_LIB} ${GUROBI_BASE_LIB})
