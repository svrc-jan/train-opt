#include <filesystem>

#include <iostream>

#include "utils/stl_print.hpp"
#include "instance.hpp"
#include "preprocess.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	for (const auto& entry : filesystem::directory_iterator("data/")) {
		if (entry.is_directory()) {
			continue;
		}

		Instance inst(entry.path());
		Preprocess prepr(inst);
	}


	return 0;
}
