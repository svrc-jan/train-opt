#include "instance.hpp"

#include <set>
#include <queue>
#include <print>
#include "utils/files.hpp"

using namespace std;

Instance::Instance(const string& file_name) 
{
	json inst_jsn = get_json_file(file_name);

	this->prepare(inst_jsn);
	this->parse(inst_jsn);
	this->assign_arrays();
	this->assign_pred_ops();
	this->propagate_lower_bounds();
	this->set_max_bound();
	this->propagate_upper_bounds();
}


void Instance::prepare(json inst_jsn)
{
	int ops_size = 0;
	int trains_size = 0;
	int succ_size = 0;
	int op_res_size = 0;
	int objs_size = 0;

	for (const json& train_jsn : inst_jsn["trains"]) {
		trains_size++;

		for (const json& op_jsn : train_jsn) {
			ops_size++;

			succ_size += op_jsn["successors"].size();
			if (op_jsn.contains("resources")) {
				op_res_size += op_jsn["resources"].size();
				for (const auto& res_jsn : op_jsn["resources"]) {
					this->add_res_name(res_jsn["resource"]);
				}
			}
		}
	}

	objs_size = inst_jsn["objective"].size();


	this->ops.reserve(ops_size);
	this->trains.reserve(trains_size);
	this->op_succ.reserve(succ_size);
	this->op_res.reserve(op_res_size);
	this->objs.reserve(objs_size);
}


void Instance::parse(json inst_jsn)
{
	set<int> res_set;
	int n_res_duplicates = 0;

	for (const json& train_jsn : inst_jsn["trains"]) {
		Train train;
		train.op_start = this->n_ops();

		for (const json& op_jsn : train_jsn) {
			Op op;
			
			op.train = this->n_trains();

			op.dur = op_jsn["min_duration"];
			
			if (op_jsn.contains("start_lb")) {
				op.start_lb = op_jsn["start_lb"];
			}

			if (op_jsn.contains("start_ub")) {
				op.start_ub = op_jsn["start_ub"];
			}

			op.succ.size = 0;
			for (int s : op_jsn["successors"]) {
				op.succ.size += 1;
				this->op_succ.push_back(s + train.op_start);
			}

			op.res.size = 0;
			if (op_jsn.contains("resources")) {
				res_set.clear();

				for (const auto& res_jsn : op_jsn["resources"]) {
					Res res;

					res.idx = this->res_name_to_idx[res_jsn["resource"]];
					if (res_jsn.contains("release_time")) {
						res.time = res_jsn["release_time"];
					}

					if (res_set.find(res.idx) != res_set.end()) {
						n_res_duplicates += 1;
						continue;
					}
					res_set.insert(res.idx);

					op.res.size += 1;
					this->op_res.push_back(res);
				}
			}

			train.ops.size += 1;
			this->ops.push_back(op);
		}

		this->max_n_train_ops = max(this->max_n_train_ops, train.ops.size);
		this->trains.push_back(train);
	}

	this->op_res.shrink_to_fit();

	vector<int> op_obj_idx(this->n_ops(), -1);

	for (const json& obj_jsn : inst_jsn["objective"]) {
		assert(obj_jsn["type"] == "op_delay");

		Obj obj;
		if (obj_jsn.contains("threshold")) {
			obj.threshold = obj_jsn["threshold"];
		}

		if (obj_jsn.contains("coeff")) {
			obj.coeff = obj_jsn["coeff"];
		}

		if (obj_jsn.contains("increment")) {
			obj.increment = obj_jsn["increment"];
		}

		if (obj.coeff == 0 && obj.increment == 0) {
			continue;
		}


		int train_i = obj_jsn["train"];
		int op_i = obj_jsn["operation"];

		int op_idx = this->trains[train_i].op_start + op_i;

		op_obj_idx[op_idx] = this->objs.size();
		this->objs.push_back(obj);
	}

	for (int o = 0; o < this->n_ops(); o++) {
		if (op_obj_idx[o] >= 0) {
			this->ops[o].obj = &(this->objs[op_obj_idx[o]]);
		}
	}

}

void Instance::assign_arrays()
{
	int ops_idx = 0;
	int op_succ_idx = 0;
	int op_res_idx = 0;

	for (Train& train : this->trains) {
		assert(train.op_start == ops_idx);
		train.ops.assign_ptr(this->ops, ops_idx);

		for (Op& op : train.ops) {
			op.succ.assign_ptr(this->op_succ, op_succ_idx);
			op.res.assign_ptr(this->op_res, op_res_idx);
		}
	}

	assert(ops_idx == this->n_ops());
	assert(op_succ_idx == this->n_op_succ());
	assert(op_res_idx == this->n_op_res());
}


void Instance::assign_pred_ops()
{
	this->op_pred.resize(this->n_op_succ());

	for (const Op& op : this->ops) {
		for (int s : op.succ) {
			this->ops[s].pred.size += 1;
		}
	}

	int idx = 0;
	for (Op& op : this->ops) {
		op.pred.assign_ptr(this->op_pred, idx);
	}
	assert(idx == this->n_op_pred());

	vector<int> pred_filled(this->n_ops(), 0);
	for (int o = 0; o < this->n_ops(); o++) {
		for (int s : this->ops[o].succ) {
			this->ops[s].pred[pred_filled[s]++] = o;
		}
	}

	for (int o = 0; o < this->n_ops(); o++) {
		assert(this->ops[o].pred.size == pred_filled[o]);
	}
}


void Instance::propagate_lower_bounds()
{
	queue<int> q;

	vector<int> n_in(this->n_ops());
	for (int o = 0; o < this->n_ops(); o++) {
		n_in[o] = this->ops[o].pred.size;
	}

	for (int t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_start);

		while (!q.empty()) {
			int o = q.front();
			q.pop();

			auto& op = this->ops[o];

			for (int s : op.succ) {
				n_in[s] -= 1;
				if (n_in[s] == 0) {
					q.push(s);
				}
			}
			
			if (op.pred.size > 0) {
				int path_bound = INT_MAX;
				for (int p : op.pred) {
					auto& pred = this->ops[p];
					path_bound = min(path_bound, pred.start_lb + pred.dur);
				}

				op.start_lb = max(op.start_lb, path_bound);
			}
		}
	}
}


void Instance::propagate_upper_bounds()
{
	queue<int> q;

	vector<int> n_out(this->n_ops());
	for (int o = 0; o < this->n_ops(); o++) {
		n_out[o] = this->ops[o].succ.size;
	}

	for (int t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_last());

		while (!q.empty()) {
			int o = q.front();
			q.pop();

			auto& op = this->ops[o];

			for (int p : op.pred) {
				n_out[p] -= 1;
				if (n_out[p] == 0) {
					q.push(p);
				}
			}
			
			if (op.succ.size > 0) {
				int path_bound = 0;
				for (int s : op.succ) {
					auto& succ = this->ops[s];
					path_bound = max(path_bound, succ.start_ub - op.dur);
				}

				op.start_ub = min(op.start_ub, path_bound);
			}
		}
	}
}


void Instance::set_max_bound()
{
	queue<int> q;

	vector<int> n_out(this->n_ops());
	for (int o = 0; o < this->n_ops(); o++) {
		n_out[o] = this->ops[o].succ.size;
	}

	vector<int> dist(this->n_ops(), 0);

	int total_dur = 0;
	vector<int> train_dur(this->n_trains());

	for (int t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_last());

		while (!q.empty()) {
			int o = q.front();
			q.pop();

			for (int p : this->ops[o].pred) {
				dist[p] = max(dist[p], dist[o] + this->ops[p].dur);

				n_out[p] -= 1;
				if (n_out[p] == 0) {
					q.push(p);
				}
			}
		}

		train_dur[t] = dist[train.op_start];
		total_dur += train_dur[t];
	}
	

	int max_bound = 0;
	for (int o = 0; o < this->n_ops(); o++) {
		auto& op = this->ops[o];

		int op_bound = op.start_lb + dist[o] + total_dur - train_dur[op.train];
		max_bound = max(max_bound, op_bound);
	}

	for (auto& op : this->ops) {
		if (op.start_ub == INT_MAX) {
			op.start_ub = max_bound;
		}
	}
}




void Instance::add_res_name(string res_name)
{
	if (this->res_name_to_idx.find(res_name) == this->res_name_to_idx.end()) {
		this->res_name_to_idx[res_name] = this->n_res();
	}
}