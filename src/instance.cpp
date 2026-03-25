#include "instance.hpp"

#include <cstdlib>
#include <stdexcept>
#include <set>
#include <queue>

#include "utils/optim_defs.h"
#include "utils/files.hpp"

using namespace std;

Instance::Instance(const string& file_name) 
{
	json inst_jsn = get_json_file(file_name);

	this->prepare(inst_jsn);
	this->parse(inst_jsn);
	this->assign_arrays();
	this->assign_pred_ops();
	this->set_leading_trailing();
	this->propagate_lower_bounds();
	this->set_max_bound();
	this->propagate_upper_bounds();
}


Instance::~Instance()
{
	if (this->data_ptr != nullptr) {
		free(this->data_ptr);
	}
}


void Instance::prepare(json inst_jsn)
{
	this->trains.clear();
	this->ops.clear();
	this->op_res.clear();
	this->op_succ.clear();

	set<idx_t> res_set;
	int n_res_dups = 0;

	for (const json& train_jsn : inst_jsn["trains"]) {
		this->trains.increment_size(1);

		for (const json& op_jsn : train_jsn) {
			this->ops.increment_size(1);

			this->op_succ.increment_size(op_jsn["successors"].size());
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
					this->op_res.increment_size(1);
				}
			}
		}
	}

	// if (n_res_dups > 0) {
	// 	cout << "res doups: " << n_res_dups;
	// }

	this->objs.set_size(inst_jsn["objective"].size());
	this->op_pred.set_size(this->op_succ.size());

	size_t data_size = 
		this->trains.n_bytes()	+
		this->ops.n_bytes()		+
		this->objs.n_bytes()	+
		this->op_res.n_bytes()	+
		this->op_succ.n_bytes() +
		this->op_pred.n_bytes();

	this->data_ptr = malloc(data_size);
	if (this->data_ptr == nullptr) {
		throw bad_alloc();
	}

	this->trains = this->data_ptr;
	this->ops = this->trains.end();
	this->op_res = this->ops.end();
	this->op_succ = this->op_res.end();
	this->op_pred = this->op_succ.end();
	this->objs = this->op_pred.end();
}


void Instance::parse(json inst_jsn)
{
	this->trains.clear();
	this->ops.clear();
	this->op_res.clear();
	this->op_succ.clear();

	set<idx_t> res_set;

	for (const json& train_jsn : inst_jsn["trains"]) {
		Train train;
		train.idx = this->n_trains();
		train.op_first = this->n_ops();

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

			op.succ.clear();
			for (int s : op_jsn["successors"]) {
				op.succ.increment_size(1);
				this->op_succ.push_back(s + train.op_first);
			}

			op.res.clear();
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
							Res res_find = this->op_res[this->op_res.size() - i - 1];
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

						op.res.increment_size(1);
						this->op_res.push_back(res);
					}
					
				}
			}

			// assert(op.succ.size > 0 || op.res.size == 0);

			train.ops.increment_size(1);
			this->ops.push_back(op);
		}

		this->trains.push_back(train);
	}

	this->objs.clear();

	for (const json& obj_jsn : inst_jsn["objective"]) {
		assert(obj_jsn["type"] == "op_delay");

		Obj obj;
		int train_i = obj_jsn["train"];
		int op_i = obj_jsn["operation"];

		obj.op = this->trains[train_i].op_first + op_i;
		
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

	for (size_t i = 0; i < this->objs.size(); i++) {
		this->ops[this->objs[i].op].obj = i;
	}

}

void Instance::assign_arrays()
{
	size_t ops_idx = 0;
	size_t op_succ_idx = 0;
	size_t op_res_idx = 0;

	for (Train& train : this->trains) {
		assert(train.op_first == ops_idx);
		train.ops.assign_offset(this->ops, ops_idx);

		for (Op& op : train.ops) {
			op.succ.assign_offset(this->op_succ, op_succ_idx);
			op.res.assign_offset(this->op_res, op_res_idx);
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
			this->ops[s].pred.increment_size(1);
		}
	}

	size_t idx = 0;
	for (Op& op : this->ops) {
		op.pred.assign_offset(this->op_pred, idx, true);
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
		n_in[o] = this->ops[o].pred.size();
	}

	for (size_t t = 0; t < this->n_trains(); t++) {
		auto& train = this->trains[t];
		q.push(train.op_first);

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
			
			if (!op.pred.empty() > 0) {
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
		n_out[o] = this->ops[o].succ.size();
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
			
			if (!op.succ.empty()) {
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
	queue<idx_t> qs[OMP_NUM_THR];

	vector<idx_t> n_out(this->n_ops());
	OMP_STATIC
	for (size_t o = 0; o < this->n_ops(); o++) {
		n_out[o] = this->ops[o].succ.size();
	}

	vector<idx_t> op_cnt(this->n_ops());
	vector<tim_t> dist(this->n_ops());

	tim_t total_dur = 0;
	vector<tim_t> train_dur(this->n_trains());

	idx_t path_idx = 0;

	OMP_DYNAMIC
	for (size_t t = 0; t < this->n_trains(); t++) {
		auto& q = qs[OMP_THR_ID];

		auto& train = this->trains[t];

		idx_t o_last = train.op_last(); 
		op_cnt[o_last] = 1;
		dist[o_last] = 0;
		q.push(o_last);

		while (!q.empty()) {
			idx_t o = q.front();
			q.pop();

			for (int p : this->ops[o].pred) {
				dist[p] = max(dist[p], dist[o] + this->ops[p].dur);
				op_cnt[p] = max(op_cnt[p], (uint16_t)(op_cnt[o] + 1));

				n_out[p] -= 1;
				if (n_out[p] == 0) {
					q.push(p);
				}
			}
		}

		train_dur[t] = dist[train.op_first];
		total_dur += train_dur[t];

		train.path_idx = path_idx;
		path_idx += op_cnt[train.op_first] - train.has_leading - train.has_trailing;
	}

	this->max_paths_len = path_idx;
	

	tim_t max_bound = 0;

	OMP_STATIC
	for (size_t o = 0; o < this->n_ops(); o++) {
		auto& op = this->ops[o];

		tim_t op_bound = op.start_lb + dist[o] + total_dur - train_dur[op.train];
		max_bound = max(max_bound, op_bound);
	}

	OMP_STATIC
	for (auto& op : this->ops) {
		if (op.start_ub == TIME_MAX) {
			op.start_ub = max_bound;
		}
	}
}


void Instance::set_leading_trailing()
{
	
	OMP_STATIC_SMALL
	for (auto& train : this->trains) {
		auto& op_first = this->ops[train.op_first];
		auto& op_last = this->ops[train.op_last()];

		train.has_leading = op_first.res.empty();
		train.has_trailing = op_last.res.empty();
	}
}


Instance::Paths Instance::get_random_paths() const
{
	Paths paths = this->get_empty_paths();

	
	OMP_DYNAMIC
	for (auto& train : this->trains) {
		auto& path = paths.ops[train.idx];

		idx_t o = train.op_first;

		while (true) {
			auto& op = this->ops[o];
			if (!op.is_leading() && !op.is_trailing()) {
				path.push_back(o);
			}

			if (op.succ.empty()) {
				break;
			}
			else if (op.succ.size() == 1) {
				o = op.succ[0];
			}
			else {
				o = op.succ[random() % op.succ.size()];
			}
		}
	}

	return paths;
}


void Instance::add_res_name(string res_name)
{
	if (this->res_name_to_idx.find(res_name) == this->res_name_to_idx.end()) {
		this->res_name_to_idx[res_name] = this->n_res();
	}
}




Instance::Paths::Paths(const Instance& inst)
{
	this->ops.resize(inst.n_trains());
	this->data.resize(inst.max_paths_len);

	OMP_STATIC_SMALL
	for (size_t t = 0; t < inst.n_trains(); t++) {
		this->ops[t].set_begin(this->data.data() + inst.trains[t].path_idx);
		this->ops[t].clear();
	}
}

Instance::Paths::~Paths()
{

}

void Instance::Paths::clear()
{
	for (auto& path : this->ops) {
		path.clear();
	}
}
