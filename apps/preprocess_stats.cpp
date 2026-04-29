#include <filesystem>

#include <iostream>

#include "preprocess.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	vector<string> entries = {};

	if (argc == 1 || strlen(argv[1]) == 0) {
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

		
	}
	else {
		for (size_t i = 1; i < argc; i++) {
			entries.push_back(argv[i]);
		}
	}

	for (const auto& entry : entries) {
		cout << entry << endl;
		Instance inst(entry);
		Preprocess prepr(inst, true);
	}

	cout << "total entries: " << entries.size() << endl;

	return 0;
}
