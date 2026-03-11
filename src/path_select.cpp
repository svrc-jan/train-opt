#include "path_select.hpp"

#include <queue>

#include "utils/stl_print.hpp"

using namespace std;


Path_select::Path_select(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{
	
}


Path_select::~Path_select()
{

}


void Path_select::select_paths(vector<vector<int>>& paths, vector<int>& junct_time)
{
	this->propagate_junct_time(junct_time);
	auto op_intervals = this->get_op_intervals(junct_time);

	int n_ops = this->inst.n_ops();
	int n_trains = this->inst.n_trains();
	
	vector<bool> train_selected(n_trains, false);
	vector<int> op_cost(n_ops);

	while (true) {
		int t_max = -1;
		int cost_max = 0;

		for (int t = 0; t < n_trains; t++) {
			if (train_selected[t]) {
				continue;
			}

			auto& train = this->inst.trains[t];

			int cost = 0;

			for (int o = train.op_start; o < train.op_end(); o++) {
				cost += op_cost[o];
			}

			if (cost_max < cost) {
				t_max = t;
				cost_max = cost;
			}
		}
	}
}


void Path_select::select_paths(vector<vector<int>>& paths)
{
	vector<int> junct_time(this->prepr.n_juncts(), -1);
	this->select_paths(paths, junct_time);
}


void Path_select::propagate_junct_time(vector<int>& junct_time)
{
	for (int l = 0; l < this->prepr.n_juncts(); l++) {
		auto& junct = this->prepr.juncts[l];

		if (junct_time[l] < 0) {
			junct_time[l] = junct.time_lb;
			
			int path_time = INT_MAX;
			
			for (int o : junct.ops_in) {
				int dur = this->inst.ops[o].dur;
				int j_pred = this->prepr.ops[o].junct_start;

				path_time = min(path_time, junct_time[j_pred] + dur);
			}

			if (path_time < INT_MAX) {
				junct_time[l] = max(junct_time[l], path_time);
			}
		}
	}
}


vector<Path_select::Interval> Path_select::get_op_intervals(
	const vector<int>& junct_time)
{
	int n_ops = 0;
	vector<Interval> intervals(n_ops);

	for (int o = 0; o < this->inst.n_ops(); o++) {
		int j_start = this->prepr.ops[o].junct_start;
		int j_end = this->prepr.ops[o].junct_end;

		intervals[o] = {junct_time[j_start], junct_time[j_end]};
	}

	return intervals;
}

struct Prio_op
{
	int op;
	int prio;

	bool operator<(const Prio_op& other) const { return this->prio > other.prio; }
};


void Path_select::select_train_path(vector<vector<int>>& paths, 
	const vector<int>& op_cost, int t)
{
	auto& path = paths[t];
	auto& train = this->inst.trains[t];
	
	vector<int> op_pred(train.ops.size, -1);
	vector<int> op_dist(train.ops.size, INT_MAX);
	op_dist[train.op_start] = 0;

	priority_queue<Prio_op> prio_queue;
	prio_queue.push({train.op_start, 0});

	
	
}