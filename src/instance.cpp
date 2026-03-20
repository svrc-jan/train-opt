#include "instance.hpp"

#include <cstdlib>
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
	this->set_leading_trailing();
}


Instance::~Instance()
{
	if (this->data_ptr != nullptr) {
		free(this->data_ptr);
	}
}


void Instance::prepare(json inst_jsn)
{
	this->trains.size = 0;
	this->ops.size = 0;
	this->op_res.size = 0;
	this->op_succ.size = 0;

	set<idx_t> res_set;
	int n_res_dups = 0;
	for (const json& train_jsn : inst_jsn["trains"]) {
		this->trains.size++;

		for (const json& op_jsn : train_jsn) {
			this->ops.size++;

			this->op_succ.size += op_jsn["successors"].size();
			if (op_jsn.contains("resources")) {
				res_set.clear();
				for (const auto& res_jsn : op_jsn["resources"]) {
					this->add_res_name(res_jsn["resource"]);

					int res_idx = this->res_name_to_idx[res_jsn["resource"]];

					if (res_set.find(res_idx) != res_set.end()) {
						n_res_dups += 1;
						continue;
					}

					res_set.insert(res_idx);
					this->op_res.size += 1;
				}
			}
		}
	}

	// if (n_res_dups > 0) {
	// 	cout << "res doups: " << n_res_dups;
	// }

	this->objs.size = inst_jsn["objective"].size();
	this->op_pred.size = this->op_succ.size;

	size_t data_size = 
		this->trains.n_bytes()	+
		this->ops.n_bytes()		+
		this->objs.n_bytes()	+
		this->op_res.n_bytes()	+
		this->op_succ.n_bytes() +
		this->op_pred.n_bytes();

	this->data_ptr = malloc(data_size);
	if (this->data_ptr == nullptr) {
		exit(1);
	}

	this->trains.set_ptr(this->data_ptr);
	this->ops.set_ptr(this->trains.end());
	this->op_res.set_ptr(this->ops.end());
	this->op_succ.set_ptr(this->op_res.end());
	this->op_pred.set_ptr(this->op_succ.end());
	this->objs.set_ptr(this->op_pred.end());
}


void Instance::parse(json inst_jsn)
{
	this->trains.size = 0;
	this->ops.size = 0;
	this->op_res.size = 0;
	this->op_succ.size = 0;

	set<idx_t> res_set;

	for (const json& train_jsn : inst_jsn["trains"]) {
		Train train;
		train.idx = this->n_trains();
		train.op_start = this->n_ops();

		for (const json& op_jsn : train_jsn) {
			Op op;
			
			op.idx = this->n_ops();
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
						bool res_found = false;
						for (size_t i = 0; i < res_set.size(); i++) {
							Res res_find = this->op_res[this->op_res.size - i - 1];
							if (res_find.idx == res.idx) {
								res_find.time = max(res_find.time, res.time);
								res_found = true;
								break;
							}
						}
						assert(res_found);	
					}
					else {
						res_set.insert(res.idx);

						op.res.size += 1;
						this->op_res.push_back(res);
					}
					
				}
			}

			// assert(op.succ.size > 0 || op.res.size == 0);

			train.ops.size += 1;
			this->ops.push_back(op);
		}

		this->trains.push_back(train);
	}

	this->objs.size = 0;

	for (const json& obj_jsn : inst_jsn["objective"]) {
		assert(obj_jsn["type"] == "op_delay");

		Obj obj;
		int train_i = obj_jsn["train"];
		int op_i = obj_jsn["operation"];

		obj.op = this->trains[train_i].op_start + op_i;
		
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

		this->objs.push_back(obj);
	}

	for (size_t i = 0; i < this->objs.size; i++) {
		this->ops[this->objs[i].op].obj = i;
	}

}

void Instance::assign_arrays()
{
	size_t ops_idx = 0;
	size_t op_succ_idx = 0;
	size_t op_res_idx = 0;

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
	for (const Op& op : this->ops) {
		for (int s : op.succ) {
			this->ops[s].pred.size += 1;
		}
	}

	size_t idx = 0;
	for (Op& op : this->ops) {
		op.pred.assign_ptr(this->op_pred, idx);
		op.pred.size = 0;
	}
	assert(idx == this->n_op_pred());

	for (size_t o = 0; o < this->n_ops(); o++) {
		for (idx_t s : this->ops[o].succ) {
			this->ops[s].pred.push_back(o);
		}
	}
}


void Instance::propagate_lower_bounds()
{
	queue<idx_t> q;

	vector<idx_t> n_in(this->n_ops());
	for (size_t o = 0; o < this->n_ops(); o++) {
		n_in[o] = this->ops[o].pred.size;
	}

	for (size_t t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_start);

		while (!q.empty()) {
			idx_t o = q.front();
			q.pop();

			auto& op = this->ops[o];

			for (idx_t s : op.succ) {
				n_in[s] -= 1;
				if (n_in[s] == 0) {
					q.push(s);
				}
			}
			
			if (op.pred.size > 0) {
				tim_t path_bound = UINT32_MAX;
				for (idx_t p : op.pred) {
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
	queue<idx_t> q;

	vector<idx_t> n_out(this->n_ops());
	for (size_t o = 0; o < this->n_ops(); o++) {
		n_out[o] = this->ops[o].succ.size;
	}

	for (size_t t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_last());

		while (!q.empty()) {
			idx_t o = q.front();
			q.pop();

			auto& op = this->ops[o];

			for (idx_t p : op.pred) {
				n_out[p] -= 1;
				if (n_out[p] == 0) {
					q.push(p);
				}
			}
			
			if (op.succ.size > 0) {
				tim_t path_bound = 0;
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
	queue<idx_t> q;

	vector<idx_t> n_out(this->n_ops());
	for (size_t o = 0; o < this->n_ops(); o++) {
		n_out[o] = this->ops[o].succ.size;
	}

	vector<tim_t> dist(this->n_ops(), 0);

	tim_t total_dur = 0;
	vector<tim_t> train_dur(this->n_trains());

	for (size_t t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_last());

		while (!q.empty()) {
			idx_t o = q.front();
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
	

	tim_t max_bound = 0;
	for (size_t o = 0; o < this->n_ops(); o++) {
		auto& op = this->ops[o];

		tim_t op_bound = op.start_lb + dist[o] + total_dur - train_dur[op.train];
		max_bound = max(max_bound, op_bound);
	}

	for (auto& op : this->ops) {
		if (op.start_ub == TIME_MAX) {
			op.start_ub = max_bound;
		}
	}
}


void Instance::set_leading_trailing()
{
	for (auto& train : this->trains) {
		auto& op_start = this->ops[train.op_start];
		auto& op_last = this->ops[train.op_last()];

		train.has_leading = op_start.res.size == 0;
		train.has_trailing = op_last.res.size == 0;
	}
}



void Instance::add_res_name(string res_name)
{
	if (this->res_name_to_idx.find(res_name) == this->res_name_to_idx.end()) {
		this->res_name_to_idx[res_name] = this->n_res();
	}
}