#include <filesystem>
#include <iostream>

#include "utils/stl_print.hpp"
#include "instance.hpp"

using namespace std;


int main(int argc, char const *argv[])
{

	vector<string> entries = {};

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

	for (const auto& entry : entries) {
		cout << entry << endl;
		Instance inst(entry, true);
	}

	return 0;
}
