#include "instance.hpp"

#include <cstdlib>
#include <stdexcept>
#include <set>
#include <queue>

using namespace std;

Instance::Instance(const string& file_name, const bool verify)
{
	json inst_jsn = get_json_from_file(file_name);

	this->prepare(inst_jsn);
	this->parse(inst_jsn);
	this->assign_arrays();
	this->assign_pred_ops();

	if (verify) {
		this->verify_json(inst_jsn);
		this->verify_pred();
	}

	cout << "Instance" <<
		"\n  trains:  " << this->n_trains() <<
		"\n  objs:    " << this->objs.size() << 
		"\n  ops:     " << this->n_ops() <<
		"\n  op succ: " << this->n_op_succ() <<
		"\n  res:     " << this->n_res << 
		"\n  op res:  " << this->n_op_res() << endl;
}


Instance::~Instance()
{
	if (this->data_ptr != nullptr) {
		free(this->data_ptr);
	}
}


void Instance::prepare(const json& inst_jsn)
{
	this->trains.clear();
	this->ops.clear();
	this->op_res.clear();
	this->op_succ.clear();

	set<idx_t> res_set;
	size_t n_res_dups = 0;


	for (const json& train_jsn : inst_jsn["trains"]) {
		this->trains.increment_size(1);

		for (const json& op_jsn : train_jsn) {
			this->ops.increment_size(1);

			this->op_succ.increment_size(op_jsn["successors"].size());
			if (op_jsn.contains("resources")) {
				res_set.clear();
				for (const auto& res_jsn : op_jsn["resources"]) {
					string res_name = res_jsn["resource"];
					idx_t res_idx = this->add_res_name(res_name);
					
					auto ret = res_set.insert(res_idx);
					if (ret.second) {					
						this->op_res.increment_size(1);
					}
					else {
						n_res_dups += 1;
					}
				}
			}
		}
	}

	assert(this->n_ops() < IDX_MAX);

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


void Instance::parse(const json& inst_jsn)
{
	this->trains.clear();
	this->ops.clear();
	this->op_res.clear();
	this->op_succ.clear();

	vector<Res> res_vec;

	for (const json& train_jsn : inst_jsn["trains"]) {
		Train train;
		train.idx = this->n_trains();
		train.op_first = this->n_ops();

		for (const json& op_jsn : train_jsn) {
			Op op;
			
			op.idx = this->n_ops();
			op.train = this->n_trains();
			op.dur = op_jsn["min_duration"];
			
			json_update(op.start_lb, "start_lb", op_jsn);
			json_update(op.start_ub, "start_ub", op_jsn);

			op.succ.clear();
			for (idx_t s : op_jsn["successors"]) {
				op.succ.increment_size(1);
				assert(s + train.op_first > op.idx);
				this->op_succ.push_back(s + train.op_first);
			}

			op.res.clear();
			if (op_jsn.contains("resources")) {
				res_vec.clear();

				for (const auto& res_jsn : op_jsn["resources"]) {
					idx_t res_idx = this->get_res_idx(res_jsn["resource"]);
					assert(res_idx < this->n_res);

					dur_t res_time = 0;
					json_update(res_time, "release_time", res_jsn);

					res_vec.push_back({res_idx, res_time});
				}

				sort(res_vec.begin(), res_vec.end());
					
				for (auto& x : res_vec) {
					if (op.res.empty() || this->op_res.back().idx != x.idx) {
						op.res.increment_size(1);
						this->op_res.push_back(x);
					}
					else {
						dur_t& res_time = this->op_res.back().time;
						res_time = MAX(res_time, x.time);
					}
				}
			}

			train.ops.increment_size(1);
			this->ops.push_back(op);
		}

		this->trains.push_back(train);
	}

	this->objs.clear();

	for (const json& obj_jsn : inst_jsn["objective"]) {
		assert(obj_jsn["type"] == "op_delay");

		Obj obj;
		size_t train_i = obj_jsn["train"];
		size_t op_i = obj_jsn["operation"];

		assert(train_i < this->n_trains());
		auto& train = this->trains[train_i];
		assert(op_i < train.n_ops());

		obj.op = train.op_first + op_i;
		json_update(obj.threshold, "threshold", obj_jsn);
		json_update(obj.coeff,     "coeff",     obj_jsn);
		json_update(obj.increment, "increment", obj_jsn);

		assert(obj.coeff == 0 || obj.increment == 0);

		if (obj.coeff == 0 && obj.increment == 0) {
			continue;
		}

		this->objs.push_back(obj);
	}

	for (auto i : this->objs_range()) {
		this->ops[this->objs[i].op].obj = i;
	}
}


void Instance::verify_json(const json& inst_jsn) const
{
	set<idx_t> res_set;

	idx_t t = 0;
	idx_t o = 0;

	for (const json& train_jsn : inst_jsn["trains"]) {
		auto& train = this->trains[t];
		assert(train.idx == t && train.op_first == o);

		for (const json& op_jsn : train_jsn) {
			auto& op = this->ops[o];
			assert(op.idx == o && op.train == t);

			assert(op.dur == op_jsn["min_duration"]);
			
			tim_t start_lb = 0;
			json_update(start_lb, "start_lb", op_jsn);
			assert(op.start_lb == start_lb);

			tim_t start_ub = TIM_MAX;
			json_update(start_ub, "start_ub", op_jsn);
			assert(op.start_ub == start_ub);

			assert(op.n_succ() == op_jsn["successors"].size());
			idx_t i = 0;
			for (idx_t s : op_jsn["successors"]) {
				assert(op.succ[i++] == s + train.op_first);
			}

			assert(op.succ.is_asc());

			if (op_jsn.contains("resources")) {
				res_set.clear();
				for (const auto& res_jsn : op_jsn["resources"]) {
					const string res_name = res_jsn["resource"];

					idx_t res_idx = this->get_res_idx(res_name);
					assert(res_idx != IDX_MAX);

					dur_t res_time = 0;
					json_update(res_time, "release_time", res_jsn);
					
					auto find_ptr = op.res.find_asc(res_idx);
					assert(find_ptr != nullptr && find_ptr->time == res_time);

					res_set.insert(res_idx);
				}
				assert(op.res.size() == res_set.size());
			}
			else {
				assert(op.res.size() == 0);
			}
			
			o++;
		}

		assert(train.op_after() == o);
		t++;
	}

	for (auto& op : this->ops) {
		assert(op.obj == IDX_MAX || this->objs[op.obj].op == op.idx);
	}

	size_t k = 0;
	for (const json& obj_jsn : inst_jsn["objective"]) {
		tim_t threshold = 0;
		uint8_t coeff = 0;
		uint8_t increment = 0;

		json_update(threshold, "threshold", obj_jsn);
		json_update(coeff,     "coeff",     obj_jsn);
		json_update(increment, "increment", obj_jsn);

		if (coeff == 0 && increment == 0) {
			continue;
		}

		auto& obj = this->objs[k];

		idx_t train_i = obj_jsn["train"];
		idx_t op_i = obj_jsn["operation"];
		idx_t o = this->trains[train_i].op_first + op_i;
		
		assert(this->ops[o].obj == k);
		assert(obj.threshold == threshold && obj.coeff == coeff && obj.increment == increment);

		k++;
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

	for (auto& op : this->ops) {
		for (idx_t s : op.succ) {
			this->ops[s].pred.push_back(op.idx);
		}
	}
}


void Instance::verify_pred()
{
	for (const Op& op : this->ops) {
		for (auto& s : op.succ) {
			assert(this->ops[s].pred.find_asc(op.idx) != nullptr);
		}
		for (auto& p : op.pred) {
			assert(this->ops[p].succ.find_asc(op.idx) != nullptr);
		}
	}
}


void Instance::propagate_lower_bounds()
{
	queue<idx_t> q;

	vector<idx_t> n_in(this->n_ops());
	for (auto o : this->ops_range()) {
		n_in[o] = this->ops[o].pred.size();
	}

	for (auto& train : this->trains) {
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

	for (auto o : this->ops_range()) {
		n_out[o] = this->ops[o].succ.size();
	}

	for (auto& train : this->trains) {
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
	queue<idx_t> q;

	vector<idx_t> n_out(this->n_ops());
	for (auto o : this->ops_range()) {
		n_out[o] = this->ops[o].succ.size();
	}

	vector<idx_t> op_cnt(this->n_ops());
	vector<tim_t> dist(this->n_ops());

	tim_t total_dur = 0;
	vector<tim_t> train_dur(this->n_trains());

	idx_t path_idx = 0;

	for (auto& train : this->trains) {
		q.push(train.op_last());

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

		train_dur[train.idx] = dist[train.op_first];
		total_dur += train_dur[train.idx];

		train.path_idx = path_idx;
		path_idx += op_cnt[train.op_first];
	}

	this->max_paths_len = path_idx;
	

	tim_t max_bound = 0;

	for (auto& op : this->ops) {

		tim_t op_bound = op.start_lb + dist[op.idx] + total_dur - train_dur[op.train];
		max_bound = max(max_bound, op_bound);
	}

	for (auto& op : this->ops) {
		if (op.start_ub == TIM_MAX) {
			op.start_ub = max_bound;
		}
	}
}


bool Instance::is_op_lock(idx_t o, idx_t r) const
{
	auto& op = this->ops[o];
	if (op.res.find(r) == nullptr) {
		return false;
	}

	if (op.n_pred() == 0) {
		return true;
	}

	for (auto p : op.pred) {
		if (this->ops[p].res.find(r) == nullptr) {
			return true;
		}
	}

	return false;
}


bool Instance::is_op_unlock(idx_t o, idx_t r) const
{
	auto& op = this->ops[o];
	if (op.res.find(r) == nullptr) {
		return false;
	}

	if (op.n_succ() == 0) {
		return true;
	}

	for (auto s : op.succ) {
		if (this->ops[s].res.find(r) == nullptr) {
			return true;
		}
	}

	return false;
}



Instance::Paths Instance::get_random_paths() const
{
	Paths paths = this->get_empty_paths();

	for (auto& train : this->trains) {
		auto& path = paths.ops[train.idx];

		idx_t o = train.op_first;

		while (true) {
			auto& op = this->ops[o];
			path.push_back(o);

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


Instance::idx_t Instance::add_res_name(const std::string& res_name)
{
	idx_t res_idx = get_res_idx(res_name);

	if (res_idx == IDX_MAX) {
		res_idx = this->n_res++;
		assert(res_idx == this->res_name_to_idx.size() && res_idx < IDX_MAX);

		this->res_name_to_idx[res_name] = res_idx;
	}

	return res_idx;
}


Instance::idx_t Instance::get_res_idx(const std::string& res_name) const
{
	auto& mp = this->res_name_to_idx; 

	auto it = mp.find(res_name);
	idx_t idx = (it == mp.end()) ? IDX_MAX : it->second;

	return idx;
}


Instance::Paths::Paths(const Instance& inst)
{
	this->ops.resize(inst.n_trains());
	this->data.resize(inst.max_paths_len);

	for (auto t : inst.trains_range()) {
		this->ops[t].set_begin(this->data.data() + inst.trains[t].path_idx, false);
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
