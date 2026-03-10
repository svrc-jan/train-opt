#include <iostream>
#include <chrono>

#include "instance.hpp"
#include "preprocess.hpp"
#include "path_select.hpp"
#include "utils/stl_print.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/wab_large_1.json";

	if (argc > 1 && strlen(argv[1]) > 0) {
		file_name = string(argv[1]);
	}

	GRBEnv grb_env = GRBEnv();
	// grb_env.set(GRB_IntParam_OutputFlag, 0);

	cout << file_name << endl;
	Instance inst(file_name);
	Preprocess prepr(inst);
	Path_select path_sel(prepr, grb_env);

	vector<vector<int>> paths;
	path_sel.select_paths(paths);

	for (auto& path : paths) {
		cout << path << endl;
	}

	return 0;
}
