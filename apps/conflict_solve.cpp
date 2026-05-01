#include <filesystem>

#include <iostream>
#include <chrono>

#include "preprocess.hpp"
#include "link_graph.hpp"
#include "chunk_manager.hpp"
#include "route_planner.hpp"
#include "conflict_resolver.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	vector<string> entries = {};

	if (argc == 1 || strlen(argv[1]) == 0) {
		for (const auto& dir_entry : filesystem::directory_iterator("data/")) {
			if (dir_entry.is_directory()) {
				for (const auto& file_entry : filesystem::directory_iterator(dir_entry)) {
					entries.push_back(file_entry.path());
				}
			}
			else {
				entries.push_back(dir_entry.path());
			}
		}

		sort(entries.begin(), entries.end());
	}
	else {
		for (int i = 1; i < argc; i++) {
			entries.push_back(argv[i]);
		}
	}

	GRBEnv grb_env = GRBEnv(true);
	try {
		grb_env.set(GRB_IntParam_OutputFlag, 0);
		grb_env.set(GRB_IntParam_Threads, 8);
	}
	catch (const GRBException& ex) {
		cout << "exception: " << ex.getMessage();
		exit(1);
	}
	grb_env.start();


	for (const auto& entry : entries) {
		cout << entry << endl;

		auto t1 = chrono::steady_clock::now();
		Instance inst(entry);
		auto t2 = chrono::steady_clock::now();
		cout << "instance: " << round(chrono::duration<double, milli>(t2 - t1).count()) << "ms" << endl;

		t1 = chrono::steady_clock::now();
		Preprocess prepr(inst);
		t2 = chrono::steady_clock::now();
		cout << "preprocess: " << round(chrono::duration<double, milli>(t2 - t1).count()) << "ms" << endl;
		
		t1 = chrono::steady_clock::now();
		Link_graph link_graph(prepr);
		t2 = chrono::steady_clock::now();
		cout << "link graph: " << round(chrono::duration<double, milli>(t2 - t1).count()) << "ms" << endl;

		t1 = chrono::steady_clock::now();
		Chunk_manager chunk_mngr(prepr);
		Route_planner route_plnr(prepr, link_graph, chunk_mngr, grb_env);

		route_plnr.estimate_level_times();
		route_plnr.make_train_conflicts();
		route_plnr.make_init_routes();
		cout << "random loss: " << route_plnr.get_cost_sum() << endl;
		Flag ops_random;
		ops_random = route_plnr.op_active;

		route_plnr.optimize_routes();
		cout << "optimize loss: " << route_plnr.get_cost_sum() << endl;
		Flag ops_optim;
		ops_optim = route_plnr.op_active;

		t2 = chrono::steady_clock::now();
		cout << "routing: " << round(chrono::duration<double, milli>(t2 - t1).count()) << "ms" << endl;

		Conflict_resolver conf_rslvr(prepr, link_graph, chunk_mngr, grb_env);

		cout << "random solve" << endl;
		conf_rslvr.set_ops(ops_random);
		conf_rslvr.solve();

		cout << "optim solve" << endl;
		conf_rslvr.clear_all();
		conf_rslvr.set_ops(ops_optim);
		conf_rslvr.solve();
	}

	if (entries.size() > 1) {
		cout << "total entries: " << entries.size() << endl;
	}

	return 0;
}
