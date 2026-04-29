#include <filesystem>

#include <iostream>

#include "preprocess.hpp"
#include "link_graph.hpp"
#include "chunk_manager.hpp"
#include "route_planner.hpp"

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
		grb_env.set(GRB_IntParam_ThreadLimit, 8);
		grb_env.set(GRB_IntParam_Threads, 8);
	}
	catch (const GRBException& ex) {
		cout << "exception: " << ex.getMessage();
		exit(1);
	}
	grb_env.start();


	for (const auto& entry : entries) {
		cout << entry << endl;
		
		Instance inst(entry);
		Preprocess prepr(inst, true);
		Link_graph link_graph(prepr);
		Chunk_manager chunk_mngr(prepr);
		Route_planner route_plnr(prepr, link_graph, chunk_mngr, grb_env);

		route_plnr.make_init_routes();
		link_graph.print_chains();
	}

	cout << "total entries: " << entries.size() << endl;

	return 0;
}
