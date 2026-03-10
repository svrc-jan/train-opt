#include <print>
#include <random>
#include <iostream>
#include <algorithm>

#include "utils/stl_print.hpp"
#include "path_selector.hpp"
#include "solver.hpp"


using namespace std;

int main(int argc, char const *argv[])
{
	string file_name = "data/nor1_full_0.json";

	if (argc > 1) {
		file_name = string(argv[1]);
	}

	int n_iter = 1;
	if (argc > 2) {
		n_iter = atoi(argv[2]);
	}

	double delay_lr = 0.02;
	if (argc > 3) {
		delay_lr = atof(argv[3]);
	}

	double rnd_mix = 0.5;
	if (argc > 4) {
		delay_lr = atof(argv[4]);
	}
	

	print("{}\n", file_name);

	Instance inst(file_name);
	Path_selector path_sel(inst);
	Preprocess prepr(inst);
	Graph graph(prepr);
	Schedule sched(graph);
	Solver solver(sched);

	vector<vector<int>> paths;
	path_sel.select_all_paths_by_res_imp(paths);

	double prio_adjust = 0.001;

	vector<double> train_order(inst.n_trains());
	for (int t = 0; t < inst.n_trains(); t++) {
		graph.set_path(paths[t]);
		sched.set_path(t, paths[t]);
		train_order[t] = ((double)t)/(inst.n_trains() - 1);
	}

	std::random_device rd;
	std::mt19937 g(rd());


	double best_avg_delay = __DBL_MAX__;

	
	vector<double> train_delay_avg(inst.n_trains(), 0.0);

	double train_delay_avg_max = 0.0;
	for (int iter = 0; iter < n_iter; iter++) {
		shuffle(train_order.begin(), train_order.end(), g);

		vector<double> train_prio(inst.n_trains(), 0);
		for (int t = 0; t < inst.n_trains(); t++) {
			train_prio[t] += rnd_mix*train_order[t];
			if (train_delay_avg_max > 0) {
				train_prio[t] += (1 - rnd_mix)*train_delay_avg[t]/train_delay_avg_max;
			}
		}

		// cout << train_prio << endl;

		graph.clear_constrains();
		if (!solver.solve_with_train_prio(train_prio)) {
			break;
		}
	
		double delay_sum = 0;

		train_delay_avg_max = 0;
		vector<int> train_delay(inst.n_trains());
		for (int t = 0; t < inst.n_trains(); t++) {
			int o_last = inst.trains[t].op_last();
			auto& op_last = inst.ops[o_last];
			int lvl_last = prepr.op_level[o_last].first;

			double delay = op_last.start_lb;
			delay_sum += delay;


			
			train_delay_avg[t] = (1-delay_lr)*train_delay_avg[t] + delay_lr*delay;
			train_delay_avg_max = max(train_delay_avg_max, train_delay_avg[t]);
		}

		double avg_delay = delay_sum/(double)inst.n_trains();		
		best_avg_delay = min(best_avg_delay, avg_delay);

		println("iter {}, curr {:.1f}, best {:.1f}", iter, avg_delay, best_avg_delay);
	}

	
	return 0;
}
