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

		if (entries.size() == 1) {
			Localize local(prepr);
			Localize local_split(prepr, false);

			cout << "choke areas:" << endl;
			for (auto& area : local.areas_choke) {
				cout << area.idx << " "  << area.res << endl;
			}
			
			cout << "branch areas:" << endl;
			for (auto& area : local.areas_branch) {
				cout << area.idx << " " << area.res << endl;
			}

			if (local.n_areas_branch() != local_split.n_areas_branch()) {
				cout << "branch areas - split:" << endl;
				for (auto& area : local_split.areas_branch) {
					cout << area.idx << " "  << area.res << endl;
				}
			}
		}
		else {
			Localize local(prepr);
			Localize local_split(prepr, false);

			cout << "choke: " << local.n_areas_choke() << "/" << local_split.n_areas_choke() <<
				", branch: " << local.n_areas_branch() << "/" << local_split.n_areas_branch() << endl;;
		}
	}
	
	return 0;
}
