add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance json_aux)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set flag)

add_library(event_graph event_graph.cpp)
target_link_libraries(event_graph flag)

add_library(solver solver.cpp)
add_library(route_planner route_planner.cpp)
add_library(conflict_resolver conflict_resolver.cpp)

target_include_directories(solver PUBLIC ${GUROBI_INC})
target_link_libraries(solver route_planner conflict_resolver
	preprocess event_graph ${GUROBI_API_LIB} ${GUROBI_BASE_LIB})

target_include_directories(route_planner PUBLIC ${GUROBI_INC})
target_link_libraries(route_planner solver)

target_include_directories(conflict_resolver PUBLIC ${GUROBI_INC})
target_link_libraries(conflict_resolver solver)

