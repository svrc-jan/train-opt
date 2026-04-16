#include <filesystem>

#include <iostream>

#include "preprocess.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	for (const auto& entry : filesystem::directory_iterator("data/")) {
		if (entry.is_directory()) {
			continue;
		}

		cout << entry.path() << endl;
		Instance inst(entry.path());
	}


	return 0;
}
