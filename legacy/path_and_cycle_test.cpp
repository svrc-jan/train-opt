#include <filesystem>

#include <iostream>
#include <chrono>

#include "utils/stl_print.hpp"
#include "path_and_cycle.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/wab_large_1.json";

	if (argc > 1 && strlen(argv[1]) > 0) {
		file_name = string(argv[1]);
	}

	cout << file_name << endl;
	Instance inst(file_name);
	Path_and_cycle pnc(inst);

	srandom(43);

	pnc.set_paths(inst.get_random_paths());
	pnc.solve();

	return 0;
}
