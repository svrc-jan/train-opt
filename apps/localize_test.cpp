#include <filesystem>

#include <iostream>

#include "localize.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
	vector<string> entries = {};
	if (argc == 1 || strlen(argv[1]) == 0) {
		for (const auto& entry : filesystem::directory_iterator("data/")) {
			if (entry.is_directory()) {
				continue;
			}

			entries.push_back(entry.path());
		}

		sort(entries.begin(), entries.end());
	}
	else {
		entries.push_back(argv[1]);
	}

	for (const auto& entry : entries) {
		cout << entry << endl;
		Instance inst(entry);
		Preprocess prepr(inst);
		Localize local(prepr);

		if (entries.size() == 1) {
			for (auto& area : local.areas) {
				auto typ = (area.typ == AREA_CHOKE) ? " choke  " : " branch ";
				cout << area.idx << typ << area.res << endl;
			}
		}
	}
	
	return 0;
}
