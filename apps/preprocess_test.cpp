#include <filesystem>

#include <iostream>

#include "utils/stl_print.hpp"
#include "instance.hpp"
#include "preprocess.hpp"

using namespace std;

int main(int argc, char const *argv[])
{

	string file_name = "data/wab_large_1.json";

	if (argc > 1 && strlen(argv[1])) {
		file_name = string(argv[1]);
	}

	cout << file_name << endl;
	Instance inst(file_name);
	Preprocess prepr(inst);

	return 0;
}
