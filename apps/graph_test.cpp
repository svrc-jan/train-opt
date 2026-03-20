#include <filesystem>

#include <iostream>
#include <chrono>

#include "utils/stl_print.hpp"
#include "instance.hpp"
#include "preprocess.hpp"
#include "graph.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/wab_large_1.json";

	if (argc > 1 && strlen(argv[1]) > 0) {
		file_name = string(argv[1]);
	}

	auto t1 = chrono::high_resolution_clock::now();

	cout << file_name << endl;
	Instance inst(file_name);
	Preprocess prepr(inst);

	auto t2 = chrono::high_resolution_clock::now();

	auto dur1 = (t2 - t1).count();

	Graph graph;


	vector<uint32_t> lower_bound(prepr.n_juncts());
	
	t1 = chrono::high_resolution_clock::now();
	graph.set_vertices(prepr.n_juncts());
	for (size_t j = 0; j < prepr.n_juncts(); j++) {
		auto& junct = prepr.juncts[j];
		lower_bound[j] = junct.time_lb; 
		
		for (auto& succ : junct.succ) {
			auto dur = inst.ops[succ.op].dur;
			graph.add_edge(j, succ.junct, dur);
		}
	}
	t2 = chrono::high_resolution_clock::now();

	auto dur2 = (t2 - t1).count();

	vector<uint16_t> targets(inst.n_trains());

	for (size_t t = 0; t < inst.n_trains(); t++) {
		targets[t] = prepr.trains[t].junct_last();
	}

	graph.lower_bound = lower_bound;

	t1 = chrono::high_resolution_clock::now();
	auto& path = graph.make_path(targets);
	t2 = chrono::high_resolution_clock::now();
	
	auto dur3 = (t2 - t1).count();

	vector<uint32_t> path_len(inst.n_trains());
	
	for (size_t t = 0; t < inst.n_trains(); t++) {
		path_len[t] = path[targets[t]].time;
	}
	cout << path_len << endl;

	cout << "inst prepr:  " << dur1/1000 <<
		"\ngraph setup: " << dur2/1000 <<
		"\ngraph path:  " << dur3/1000 << endl;

	return 0;
}
