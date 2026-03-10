add_subdirectory(utils)

add_library(instance instance.cpp)
target_link_libraries(instance files)

add_library(preprocess preprocess.cpp)
target_link_libraries(preprocess instance disjoint_set)

add_library(path_selector path_selector.cpp)
target_link_libraries(path_selector instance)

add_library(graph graph.cpp)
target_link_libraries(graph preprocess instance)

add_library(schedule schedule.cpp)
target_link_libraries(schedule graph)

add_library(solver solver.cpp)
target_link_libraries(solver schedule)

add_library(lin_prog lin_prog.cpp)
target_link_libraries(lin_prog preprocess)

add_library(gurobi_model gurobi_model.cpp)
target_include_directories(gurobi_model PUBLIC ${GUROBI_INC})
target_link_libraries(gurobi_model preprocess ${GUROBI_API_LIB} ${GUROBI_BASE_LIB})
