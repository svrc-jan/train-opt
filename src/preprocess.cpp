#include "preprocess.hpp"

#include <cstdio>
#include <iostream>
#include <set>

#include "utils/disjoint_set.hpp"
#include "utils/stl_print.hpp"

using namespace std;


Preprocess::Preprocess(const Instance& inst, const bool verify) : inst(inst)
{
	this->trains.resize(this->inst.n_trains(), Train());
	this->make_junctions();
	this->make_levels();

	if (verify) {
		this->verify_juncts();
		this->verify_levels();
	}

	this->make_req_levels();
	this->make_req_ops();
	this->make_routes();
	this->make_junct_route();
}


Preprocess::~Preprocess()
{

}


void Preprocess::make_junctions()
{
	size_t n_ops = this->inst.n_ops();
	size_t n_trains = this->inst.n_trains();

	this->ops.resize(n_ops);
	for (auto o : this->inst.ops_range()) {
		this->ops[o].idx = o;
	}

	this->trains.resize(n_trains, Train());

	size_t junct_idx = 0;

	for (size_t t = 0; t < n_trains; t++) {

		auto& inst_train = this->inst.trains[t];
		auto& train = this->trains[t];
		train.idx = t;

		Disjoint_set disj_set(inst_train.ops.size());

		idx_t o_first = inst_train.op_first;
		train.ops.set_begin(&this->ops[o_first]);
		train.ops.set_size(inst_train.n_ops());

		for (auto& op : inst_train.ops) {
			for (auto it_a = op.succ.begin(); it_a < op.succ.end(); it_a++) {
				for (auto it_b = it_a + 1; it_b < op.succ.end(); it_b++) {
					int a = *it_a - o_first;
					int b = *it_b - o_first;
					disj_set.union_set(a, b);
				}
			}
		}
		
		train.junct_first = junct_idx;

		auto set_idx = disj_set.get_result();
		
		for (auto& op : train.ops) {
			op.train = train.idx;
			op.junct.start = set_idx[op.idx - o_first] + junct_idx;
		}

		train.juncts.set_size(disj_set.n_sets + 1);
		this->ops[inst_train.op_last()].junct.end = train.junct_last();	

		junct_idx += train.juncts.size();
	}

	for (auto& op : this->inst.ops) {
		auto& j_op = this->ops[op.idx].junct;
		assert(j_op.start < junct_idx);

		for (int p : op.pred) {
			auto& j_pred = this->ops[p].junct;
			if (j_pred.end == IDX_MAX) {
				j_pred.end = j_op.start;
			} 
			else {
				assert(j_pred.end == j_op.start);
			}
		}
	}

	this->juncts.resize(junct_idx, Junction());

	for (auto& op : this->ops) {
		auto& j_op = op.junct;
		assert(j_op.end < junct_idx);

		this->juncts[j_op.start].succ.increment_size(1);
		this->juncts[j_op.end].pred.increment_size(1);

		n_ops += 1;
	}

	this->junct_succ.resize(n_ops);
	this->junct_pred.resize(n_ops);

	size_t succ_idx = 0;
	size_t pred_idx = 0;

	junct_idx = 0;
	for (auto& junct : this->juncts) {
		junct.idx = junct_idx++;

		junct.succ.assign_offset(this->junct_succ, succ_idx, true);
		junct.pred.assign_offset(this->junct_pred, pred_idx, true);
	}

	for (auto& op : this->ops) {
		auto& j_op = op.junct;

		this->juncts[j_op.start].succ.push_back({j_op.end, op.idx});
		this->juncts[j_op.end].pred.push_back({j_op.start, op.idx});
	}

	junct_idx = 0;
	for (auto& train : this->trains) {
		train.juncts.assign_offset(this->juncts, junct_idx, false);
		for (auto& junct : train.juncts) {
			junct.train = train.idx;
		}
	}
}


void Preprocess::verify_juncts() const
{
	for (idx_t j = 0; j < this->n_juncts(); j++) {
		auto& junct = this->juncts[j];
		assert(junct.idx == j);

		for (auto& s : junct.succ) {
			assert(this->ops[s.op].junct.start == j);
		}

		for (auto& p : junct.pred) {
			assert(this->ops[p.op].junct.end == j);
		}
	}

	for (auto op : this->ops) {
		auto& j = op.junct;
		assert(j.start < this->n_juncts() && j.end < this->n_juncts());

		assert(this->juncts[j.start].succ.find(Idx_op(j.end, op.idx)) != nullptr);
		assert(this->juncts[j.end].pred.find(Idx_op(j.start, op.idx)) != nullptr);
	}


	for (auto& train : this->trains) {
		for (auto& juncts : train.juncts) {
			assert(juncts.train == train.idx);
		}
	}
}


void Preprocess::make_levels()
{
	idx_t in_deg[this->n_juncts()];
	for (idx_t j = 0; j < this->n_juncts(); j++) {
		in_deg[j] = this->juncts[j].n_pred();
	}

	vector<idx_t> curr = {};
	vector<idx_t> zero_in = {};

	this->levels.reserve(this->n_juncts());
	this->level_juncts.reserve(this->n_juncts());
	
	for (auto& train : this->trains) {
		train.level_first = this->n_levels();
		train.levels.set_size(0);

		zero_in.clear();
		zero_in.push_back(train.junct_first);
		
		while (!zero_in.empty()) {
			curr = zero_in;
			zero_in.clear();

			Level level;
			level.idx = this->n_levels();
			level.train = train.idx;

			level.juncts.set_size(curr.size());
			this->level_juncts.insert(this->level_juncts.end(), curr.begin(), curr.end());

			for (auto& j : curr) {
				auto& junct = this->juncts[j];
				assert(junct.level == IDX_MAX);
				junct.level = level.idx;
				
				for (auto& succ : junct.succ) {
					idx_t s = succ.idx;
					in_deg[s] -= 1;
					if (in_deg[s] == 0) {
						zero_in.push_back(s);
					}
				}
			}

			train.levels.increment_size(1);
			this->levels.push_back(level);
		}
	}

	this->levels.shrink_to_fit();
	
	size_t level_idx = 0;
	for (auto& train : this->trains) {
		train.levels.assign_offset(this->levels, level_idx, false);
		for (auto& level : train.levels) {
			assert(level.train == train.idx);
		}
	}
	
	this->level_succ.resize(this->inst.n_ops());
	this->level_pred.resize(this->inst.n_ops());

	size_t level_juncts_idx = 0;
	size_t level_succ_idx = 0;
	size_t level_pred_idx = 0;

	for (auto& level : this->levels) {
		level.succ.set_size(0);
		level.pred.set_size(0);

		level.juncts.assign_offset(this->level_juncts, level_juncts_idx, false);

		for (auto& j : level.juncts) {
			auto& junct = this->juncts[j];
			assert(junct.level == level.idx);

			level.succ.increment_size(junct.n_succ());
			level.pred.increment_size(junct.n_pred());
		}
		
		level.succ.assign_offset(this->level_succ, level_succ_idx, true);
		level.pred.assign_offset(this->level_pred, level_pred_idx, true);
	}

	for (auto& op : this->ops) {
		idx_t l_start = this->juncts[op.junct.start].level;
		idx_t l_end = this->juncts[op.junct.end].level;

		op.level.start = l_start;
		op.level.end = l_end;

		this->levels[l_start].succ.push_back({l_end, op.idx});
		this->levels[l_end].pred.push_back({l_start, op.idx});
	}
}


void Preprocess::verify_levels() const
{
	for (auto l : this->levels_range()) {
		auto& level = this->levels[l];
		assert(level.idx == l);

		for (auto& s : level.succ) {
			assert(this->ops[s.op].level.start == l);
		}

		for (auto& p : level.pred) {
			assert(this->ops[p.op].level.end == l);
		}
	}

	for (auto& op : this->ops) {
		auto& l = op.level;
		assert(l.start < this->n_levels() && l.end < this->n_levels());

		assert(this->levels[l.start].succ.find(Idx_op(l.end, op.idx)) != nullptr);
		assert(this->levels[l.end].pred.find(Idx_op(l.start, op.idx)) != nullptr);
	}


	for (auto& train : this->trains) {
		for (auto& levels : train.levels) {
			assert(levels.train == train.idx);
		}
	}
}


void Preprocess::make_req_levels()
{
	this->is_level_req.set_n_items(this->n_levels());
	this->is_level_req.fill(1);

	for (auto& level : this->levels) {
		for (auto& succ : level.succ) {
			for (idx_t l = level.idx + 1; l < succ.idx; l++) {
				this->levels[l].is_req = 0;
				this->is_level_req -= l;
			}
		}
	}
}


void Preprocess::make_req_ops()
{
	this->is_op_req.set_n_items(this->inst.n_ops());

	for (auto& train : this->trains) {
		train.ops_req.set_size(0);
	}

	for (auto& level : this->levels) {
		if (level.is_req && level.n_succ() == 1) {
			idx_t o = level.succ[0].op;
			this->is_op_req += o;
			this->trains[level.train].ops_req.increment_size(1);
		}
	}

	this->ops_req.reserve(this->is_op_req.get_true_count());
	this->ops_req.clear();
	for (auto  o : this->is_op_req.get_true_list()) {
		this->ops_req.push_back(o);
	}

	size_t req_idx = 0;
	for (auto& train : this->trains) {
		train.ops_req.assign_offset(this->ops_req, req_idx, false);
	}
}


void Preprocess::make_routes()
{
	cnt_t in_deg[this->inst.n_ops()];

	for (auto& op : this->inst.ops) {
		assert(op.n_pred() < CNT_MAX);
		in_deg[op.idx] = op.n_pred();
	}

	idx_t route_idx = 0;

	auto& q = this->que;
	
	for (auto& train_i : this->inst.trains) {
		auto& train = this->trains[train_i.idx];
		assert(q.empty());

		idx_t o_first = train_i.op_first;
		q.push(o_first);

		train.route_first = route_idx;

		while (!q.empty()) {
			idx_t o = q.front(); q.pop();
			auto& op_i = this->inst.ops[o];
			auto& op = this->ops[o];

			size_t n_pred = op_i.n_pred();
			if (this->is_op_req[o]) {
				op.route = IDX_MAX;
			}
			else if (n_pred == 1) {
				idx_t p = op_i.pred[0];
				auto& op_p = this->inst.ops[p];
				if (op_p.n_succ() == 1) {
					op.route = this->ops[p].route;
				}
				else {
					op.route = route_idx++;
				}
			}
			else {
				op.route = route_idx++;
			}

			for (auto s : op_i.succ) {
				in_deg[s] -= 1;
				if (in_deg[s] == 0) {
					q.push(s);
				}
			}
		}

		train.routes.set_size(route_idx - train.route_first);
	}

	this->routes.resize(route_idx);

	route_idx = 0;
	for (auto& train : this->trains) {
		train.routes.assign_offset(this->routes, route_idx, false);
		for (auto& route : train.routes) {
			route.train = train.idx;
		}
	}

	for (auto i : this->routes_range()) {
		auto& route = this->routes[i];
		route.idx = i;
		route.ops.set_size(0);
	}

	size_t route_ops_idx = 0;

	for (auto& op : this->ops) {
		bool is_req = this->is_op_req[op.idx];
		if (op.route < IDX_MAX) {
			assert(!is_req);
			assert(op.route < this->n_routes());

			this->routes[op.route].ops.increment_size(1);
			route_ops_idx++;
		}
		else {
			assert(is_req);
		}
	}

	this->route_ops.resize(route_ops_idx);
	assert(this->ops_req.size() + this->route_ops.size() == this->n_ops());

	route_ops_idx = 0;
	for (auto& route : this->routes) {
		route.ops.assign_offset(this->route_ops, route_ops_idx, true);
	}

	for (auto& op : this->ops) {
		if (op.route < IDX_MAX) {
			this->routes[op.route].ops.push_back(op.idx);
		}
	}
}


void Preprocess::make_junct_route()
{
	for (auto& junct : this->juncts) {
		junct.req_route_cons = false;
		for (auto& pred : junct.pred) {
			auto& op_i = this->inst.ops[pred.op];
			auto& op_p = this->ops[pred.op];

			for (auto s : op_i.succ) {
				auto& op_s = this->ops[s];
				if (op_p.route != op_s.route) {
					junct.req_route_cons = true;
				}
			}
		}
	}
}




void Preprocess::make_junction_bounds()
{
	for (auto& inst_train : this->inst.trains) {
		auto& train = this->trains[inst_train.idx];

		auto& op_last = this->inst.ops[inst_train.op_last()];
		auto& junct_last = this->juncts[train.junct_last()];
		
		junct_last.time_lb = op_last.start_lb + op_last.dur;
		junct_last.time_ub = (op_last.start_ub == TIM_MAX) ? 
			op_last.start_ub : op_last.start_ub + op_last.dur;
	}

	for (auto& junct : this->juncts) {
		if (!junct.succ.empty()) {
			junct.time_lb = UINT32_MAX;
			junct.time_ub = 0;

			for (auto& succ : junct.succ) {
				auto& op = this->inst.ops[succ.op];
				junct.time_lb = MIN(junct.time_lb, op.start_lb);
				junct.time_ub = MAX(junct.time_ub, op.start_ub);
			}
		}
	}
}


void Preprocess::make_level_bounds()
{

}


void Preprocess::make_count()
{
	size_t n_levels = this->n_levels();

	this->op_count.resize(n_levels, 0);
	this->res_count.resize(n_levels, nullptr);
	this->res_count_data.resize(n_levels*this->inst.n_res, 0);

	for (auto l : this->levels_range()) {
		this->res_count[l] = &this->res_count_data[l*this->inst.n_res];
	}

	for (auto& op : this->inst.ops) {
		for (auto l : this->ops[op.idx].level.range()) {
			
			assert(this->op_count[l] < CNT_MAX);
			this->op_count[l] += 1;
			
			for (auto& res : op.res) {
				this->res_count[l][res.idx] += 1;
			}
		}
	}
}



void Preprocess::make_level_res()
{
	size_t res_idx = 0;
	
	for (auto& level : this->levels) {
		level.res.set_size(0);
		level.res_req.set_size(0);
		level.res_opt.set_size(0);

		cnt_t o_cnt = this->op_count[level.idx];
		for (auto r : this->inst.res_range()) {
			cnt_t r_cnt = this->res_count[level.idx][r];
			
			if (r_cnt > 0) {
				level.res.increment_size(1);
				res_idx += 1;

				if (r_cnt == o_cnt) {
					level.res_req.increment_size(1);
				}
				else {
					assert(r_cnt < o_cnt);
					level.res_opt.increment_size(1);
				}
			}
		}
	}

	this->level_res.resize(res_idx);
	res_idx = 0;
	
	for (auto& level : this->levels) {
		level.res.assign_offset(this->level_res, res_idx, false);
		
		level.res_req.set_begin(level.res.begin());
		level.res_opt.set_begin(level.res.begin() + level.res_req.size(), false);

		level.res_req.clear();
		level.res_opt.clear();

		cnt_t o_cnt = this->op_count[level.idx];
		for (auto r : this->inst.res_range()) {
			cnt_t r_cnt = this->res_count[level.idx][r];
			
			if (r_cnt > 0) {
				if (r_cnt == o_cnt) {
					level.res_req.push_back(r);
				}
				else {
					level.res_opt.push_back(r);
				}	
			}
		}

		assert(level.res_req.size() + level.res_opt.size() == level.res.size());
		assert(level.res.end() == level.res_opt.end());
	}
}


void Preprocess::make_reentry_res()
{
	Flag res_entered(this->inst.n_res);
	Flag train_reentries(this->inst.n_res);

	vector<pair<pair<idx_t, idx_t>, idx_t>> reentries;

	for (auto& train : this->trains) {
		res_entered.clear();

		for (auto level : train.levels) {
			for (auto r : level.res) {
				if (res_entered[r] && this->levels[level.idx - 1].res.find(r) == nullptr) {
					reentries.push_back({{train.idx, level.idx}, r});
					train_reentries += train.idx;
				}
				res_entered += r;
			}
		}
	}

	for (auto t : train_reentries.get_true_list()) {
		for (auto& level : this->trains[t].levels) {
			cout << level.train << "." << level.idx << " - req: " << level.res_req << ", opt: " << level.res_opt << endl;
		}
	}

	cout << "res reentries: " << reentries << endl;
}



void Preprocess::make_train_res()
{
	// for (auto& train : this->trains) {
	// 	train.is_res_req.set_n_items(this->inst.n_res);
	// 	train.is_res_opt.set_n_items(this->inst.n_res);

	// 	for (auto& level : train.levels) {
	// 		for (auto& r : level.res_req) {
	// 			train.is_res_req += r;
	// 		}
	// 		for (auto& r : level.res_opt) {
	// 			train.is_res_opt += r;
	// 		}
	// 	}

	// 	train.is_res_opt.set_false(train.is_res_req);

	// 	for (auto r : this->inst.res_range()) {
	// 		assert(train.is_res_req[r] + train.is_res_opt[r] <= 1);
	// 	}
	// }
}


void Preprocess::make_global_res()
{
	this->is_res_req.set_n_items(this->inst.n_res);
	this->is_res_opt.set_n_items(this->inst.n_res);
	this->is_res_split.set_n_items(this->inst.n_res);

	for (auto& train : this->trains) {
		// this->is_res_req.set_true(train.is_res_req);
		// this->is_res_opt.set_true(train.is_res_opt);
	}

	this->is_res_split.set_true(this->is_res_req);
	this->is_res_split.mask(this->is_res_opt);

	this->is_res_req.set_false(this->is_res_split);
	this->is_res_opt.set_false(this->is_res_split);

	for (auto r : this->inst.res_range()) {
		assert(this->is_res_req[r] + this->is_res_opt[r] + this->is_res_split[r] == 1);
	}
}
