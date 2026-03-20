#include <filesystem>
#include <iostream>

#include "utils/stl_print.hpp"
#include "instance.hpp"

using namespace std;

struct Inst_stats
{
	size_t n_trains = 0;
	size_t n_ops = 0;
	size_t n_res = 0;
	uint16_t dur = 0;
	uint32_t start_ub = 0;
	uint16_t res_time = 0;
	uint32_t threshold = 0;
	uint8_t coeff = 0;
	uint8_t increment = 0;
};


int main(int argc, char const *argv[])
{

	// string file_name = "data/wab_large_1.json";
	

	// if (argc > 1 && strlen(argv[1]) > 0) {
	// 	file_name = string(argv[1]);
	// }

	Inst_stats stats;


	Instance::Paths paths_;
	for (const auto& entry : filesystem::directory_iterator("data/")) {
		if (entry.is_directory()) {
			continue;
		}

		Instance inst(entry.path());
		auto paths = inst.get_random_paths();

		stats.n_trains = max(stats.n_trains, inst.n_trains());
		stats.n_ops = max(stats.n_ops, inst.n_ops());
		stats.n_res = max(stats.n_res, inst.n_res());
		
		// tim_t max_bound = 0;

		for (auto& op : inst.ops) {
			stats.dur = max(stats.dur, op.dur);
			stats.start_ub = max(stats.start_ub, op.start_ub);


			// max_bound = max(max_bound, op.start_ub);
			

			for (auto& res : op.res) {
				stats.res_time = max(stats.res_time, res.time);
			}
		}

		// cout << entry << " " << max_bound << endl;

		for (auto& obj : inst.objs) {
			stats.threshold = max(stats.threshold, obj.threshold);
			stats.coeff = max(stats.coeff, obj.coeff);
			stats.increment = max(stats.increment, obj.increment);
		}

		paths_ = paths;
	}


	cout << "n_trains: " << stats.n_trains <<
		"\nn_ops: " << stats.n_ops <<
		"\nn_res: " << stats.n_res <<
		"\ndur: " << stats.dur <<
		"\nstart_ub: " << stats.start_ub <<
		"\nres_time: " << stats.res_time <<
		"\nthreshold: " << stats.threshold <<
		"\ncoeff:" << (uint16_t)stats.coeff <<
		"\nincrement:" << (uint16_t)stats.increment << endl;

	return 0;
}
