
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

	string entry(argv[1]);

	Instance inst(entry);
	Preprocess prepr(inst);
	Solver slvr(prepr, grb_env);

	slvr.plan_routes();
	slvr.feasible_solve();
	slvr.improving_solve();

	return 0;
}
