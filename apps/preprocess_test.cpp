#include <filesystem>

#include <iostream>

#include "preprocess.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	if (argc == 1 || strlen(argv[1]) == 0) {
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
			Instance inst(entry);
			Preprocess prepr(inst, true);
		}
	}
	else {
		Instance inst(argv[1]);
		Preprocess prepr(inst, true);

		// for (auto& level : prepr.levels) {
		// 	cout << level.train << "." << level.idx << " - req: " << level.res_req << ", opt: " << level.res_req << endl;
		// }
	}

	cout << "chunk size: " << sizeof(Preprocess::Chunk) << endl;

	return 0;
}
