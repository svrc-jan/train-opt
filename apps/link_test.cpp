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
	}
	else {
		entries.push_back(argv[1]);
	}

	for (auto& entry : entries) {
		cout << entry << endl;
		Instance inst(entry);
		Preprocess prepr(inst, true);
		Link_graph link_graph(prepr);

		for (auto& train : inst.trains) {
			uint16_t o = train.op_first;
			link_graph.link_op_self(o);

			while (true) {
				auto& op = inst.ops[o];
				if (op.n_succ() == 0) {
					break;
				}

				uint16_t s = op.succ.get_random_item();
				link_graph.link_op_succ(o, s);
				o = s;
			}
		}

		link_graph.build_all_conf_links();
		link_graph.print_chains();
	}

	return 0;
}
