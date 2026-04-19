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
	this->make_route_junct_level();
	this->make_objs();
}


Preprocess::~Preprocess()
{

}


void Preprocess::make_junctions()
{
	size_t n_ops = this->inst.n_ops();
	size_t n_trains = this->inst.n_trains();

	this->ops.resize(n_ops);
	for (auto o : this->ops_range()) {
		auto& op = this->ops[o];
		op.idx = o;
		op.inst = &this->inst.ops[o];
	}

	this->trains.resize(n_trains, Train());

	size_t junct_idx = 0;

	for (auto t : this->trains_range()) {
		auto& train = this->trains[t];
		train.idx = t;
		train.inst = &this->inst.trains[t];

		Disjoint_set disj_set(train.inst->ops.size());

		idx_t o_first = train.inst->op_first;
		train.ops.set_begin(&this->ops[o_first]);
		train.ops.set_size(train.inst->n_ops());

		for (auto& op : train.inst->ops) {
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
		this->ops[train.inst->op_last()].junct.end = train.junct_last();

		junct_idx += train.juncts.size();
	}

	for (auto& op : this->ops) {
		auto& j_op = op.junct;
		assert(j_op.start < junct_idx);

		for (int p : op.inst->pred) {
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
	for (auto j : this->juncts_range()) {
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
	
	for (auto& train : this->trains) {
		assert(q.empty());

		idx_t o_first = train.inst->op_first;
		q.push(o_first);

		train.route_first = route_idx;

		while (!q.empty()) {
			idx_t o = q.front(); q.pop();
			auto& op = this->ops[o];

			size_t n_pred = op.inst->n_pred();
			if (this->is_op_req[o]) {
				op.route = IDX_MAX;
			}
			else if (n_pred == 1) {
				idx_t p = op.inst->pred[0];
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

			for (auto s : op.inst->succ) {
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


void Preprocess::make_route_junct_level()
{
	for (auto& route : this->routes) {
		auto& op_first = this->ops[route.ops[0]];
		auto& op_last = this->ops[route.ops.back()];

		route.junct.start = op_first.junct.start;
		route.level.start = op_first.level.start;
		
		route.junct.end = op_last.junct.end;
		route.level.end = op_last.junct.end;

		this->juncts[route.junct.start].req_route_cons = true;
		this->juncts[route.junct.end].req_route_cons = true;
	}
}


void Preprocess::make_resource_chunks()
{
	size_t chunk_idx = 0;
	
	for (auto& op : this->ops) {
		if (op.inst->n_res() > 0) {
			// assert(op.inst->n_res() == 1);
		}
	}
}


void Preprocess::make_objs()
{
	this->objs.clear();
	
	vector<Obj> train_obj;

	for (auto& train : this->trains) {
		train_obj.clear();

		for (auto& op : train.ops) {
			auto& op_i = this->inst.ops[op.idx];
			if (op_i.obj == IDX_MAX) {
				continue;
			}

			auto& obj_i = this->inst.objs[op_i.obj];
			Obj obj = {
				.train = train.idx,
				.route = op.route,
				.level = op.level.start,
				.is_bin = (obj_i.increment > 0),
				.coeff = (obj_i.increment > 0) ? obj_i.increment : obj_i.coeff,
				.n_routes = 1,
				.threshold = obj_i.threshold
			};

			auto it = find(train_obj.rbegin(), train_obj.rend(), obj);
			if (it == train_obj.rend()) {
				train_obj.push_back(obj);
			}
			else {
				it->route = IDX_MAX;
				it->n_routes += 1;
			}
		}

		train.objs.set_size(train_obj.size());
		this->objs.insert(this->objs.end(), train_obj.begin(), train_obj.end());
	}

	size_t obj_idx = 0;
	for (auto& train : this->trains) {
		train.objs.assign_offset(this->objs, obj_idx, false);
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
