
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

	string entry(argv[1]);

	Instance inst(entry);
	Preprocess prepr(inst);
	Localize local(prepr);
	Solver slvr(local);

	slvr.get_init_sol();

	return 0;
}
