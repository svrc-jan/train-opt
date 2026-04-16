#include <filesystem>
#include <iostream>

#include "utils/stl_print.hpp"
#include "instance.hpp"

using namespace std;


int main(int argc, char const *argv[])
{

	vector<string> entries = {};

	for (const auto& entry : filesystem::directory_iterator("data/")) {
		if (entry.is_directory()) {
			continue;
		}

		entries.push_back(entry.path());
	}

	sort(entries.begin(), entries.end());

	for (const auto& entry : entries) {
		cout << entry << endl;
		Instance inst(entry, true);
	}

	return 0;
}
