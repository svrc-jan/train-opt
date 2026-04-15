#include <filesystem>

#include <iostream>
#include <chrono>

#include "utils/stl_print.hpp"
#include "instance.hpp"
#include "path_graph.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/nor1_full_0.json";

	if (argc > 1 && strlen(argv[1]) > 0) {
		file_name = string(argv[1]);
	}

	Instance inst(file_name);
	
	Path_graph path_graph(inst);
	auto paths = inst.get_random_paths();

	for (auto& p : paths) {
		for (auto x : p) {
			cout << x << " ";
		}
		cout << endl;
	
	}

	path_graph.set_all_paths(paths);

	size_t n_edges = 0;

	for (idx_t t1 = 0; t1 < inst.n_trains(); t1++) {
		for (idx_t t2 = 0; t2 < inst.n_trains(); t2++) {
			if (t1 == t2) {
				continue;
			}

			n_edges += path_graph.group_edges(t1, t2).size();
		}
	}

	cout << "edges: " << n_edges << endl;
 
	return 0;
}
