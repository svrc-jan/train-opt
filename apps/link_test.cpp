#include <filesystem>

#include <iostream>

#include "link_graph.hpp"

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

		for (auto& entry : entries) {
			cout << entry << endl;
			Instance inst(entry);
			Preprocess prepr(inst, true);
			Link_graph link_graph(prepr);
		}
	}
	else {

		cout << argv[1] << endl;
		Instance inst(argv[1]);
		Preprocess prepr(inst, true);
		Link_graph link_graph(prepr);

		for (auto& train : inst.trains) {
			auto o = train.op_first;
			while (true) {
				auto& op = inst.ops[o];

				if (op.n_succ() == 0) { break ; }
				auto s = op.succ.get_random_item();

				link_graph.set_op_succ(o, s);
				o = s;
			}
			
		}

		link_graph.print_chains();
	}

	return 0;
}
