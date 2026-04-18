
#include <filesystem>

#include <iostream>

#include "solver.hpp"

using namespace std;


int main(int argc, char const *argv[])
{
	vector<string> entries = {};
	if (argc == 1 || strlen(argv[1]) == 0) {
		cout << "missing instance" << endl;
		exit(1);
	}

	GRBEnv grb_env = GRBEnv();
	// grb_env.set(GRB_IntParam_OutputFlag, 0);

	string entry(argv[1]);

	Instance inst(entry);
	Preprocess prepr(inst);
	Solver slvr(prepr, grb_env);

	return 0;
}
