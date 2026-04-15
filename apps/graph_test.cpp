#include <filesystem>

#include <iostream>
#include <chrono>

#include "utils/stl_print.hpp"
#include "instance.hpp"
#include "graph.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/nor1_full_0.json";

	if (argc > 1 && strlen(argv[1]) > 0) {
		file_name = string(argv[1]);
	}

	Instance inst(file_name);
	
	Graph graph(inst.n_ops());

	for (auto& op : inst.ops) {
		for (auto s : op.succ) {
			graph.add_edge({op.idx, s, op.dur});
		}
	}

	vector<Instance::idx_t> first_ops(inst.n_trains());

	for (auto& train : inst.trains) {
		first_ops[train.idx] = train.op_first;
	}
	
	return 0;
}
