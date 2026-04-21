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

	cout << "Preprocess" << endl;
	
	this->make_junctions();
	cout << "  juncts:  " << this->n_juncts() << endl;

	this->make_levels();
	cout << "  levels:  " << this->n_levels() << endl;

	if (verify) {
		this->verify_juncts();
		this->verify_levels();
	}

	this->make_req_levels();
	this->make_req_ops();
	size_t n_opt_lvls = this->n_opt_levels();
	if (n_opt_lvls > 0) {
		cout << "    optional: " << n_opt_lvls << endl;
	}


	this->make_routes();
	this->make_route_junct_level();
	cout << "  routes:  " << this->n_routes() << endl;
	
	this->make_sections();
	cout << "  sects:   " << this->n_sects() << endl;

	this->make_resource_chunks();
	cout << "  chunks:  " << this->n_chunks() << endl;

	if (this->chunk_direct_merges > 0) {
		cout << "    direct merges:   " << this->chunk_direct_merges << endl;
	}

	if (this->chunk_parallel_merges > 0) {
		cout << "    parralel merges: " << this->chunk_parallel_merges << endl;
	}

	this->make_chunk_locks_unlocks();
	if (this->chunk_locks.size() > 0) {
		cout << "    locks:   " << this->chunk_locks.size() << endl;
	}

	if (this->chunk_unlocks.size() > 0) {
		cout << "    unlocks: " << this->chunk_unlocks.size() << endl;
	}

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


	size_t max_train_ops = 0;
	for (auto& train : this->inst.trains) {
		max_train_ops = MAX(max_train_ops, train.n_ops());
	}

	Disjoint_set disj_set;
	disj_set.reserve(max_train_ops);

	size_t junct_idx = 0;

	for (auto t : this->trains_range()) {
		auto& train = this->trains[t];
		train.idx = t;
		train.inst = &this->inst.trains[t];

		disj_set.resize(train.inst->ops.size());
		disj_set.reset();

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

		assert(this->juncts[j.start].succ.find(Junct_edge(j.end, op.idx)) != nullptr);
		assert(this->juncts[j.end].pred.find(Junct_edge(j.start, op.idx)) != nullptr);
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
		train.levels.clear();

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
					idx_t s = succ.junct;
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
		level.succ.clear();
		level.pred.clear();

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

		assert(this->levels[l.start].succ.find(Level_edge(l.end, op.idx)) != nullptr);
		assert(this->levels[l.end].pred.find(Level_edge(l.start, op.idx)) != nullptr);
	}


	for (auto& train : this->trains) {
		for (auto& levels : train.levels) {
			assert(levels.train == train.idx);
		}
	}
}


void Preprocess::make_req_levels()
{
	
	for (auto& level : this->levels) {
		level.is_req = true;
	}

	for (auto& level : this->levels) {
		for (auto& succ : level.succ) {
			for (idx_t l = level.idx + 1; l < succ.level; l++) {
				this->levels[l].is_req = false;
			}
		}
	}
}


void Preprocess::make_req_ops()
{
	this->is_op_req.set_n_items(this->inst.n_ops());

	for (auto& level : this->levels) {
		if (level.is_req && level.n_succ() == 1) {
			idx_t o = level.succ[0].op;
			this->is_op_req += o;
		}
	}

	this->ops_req.reserve(this->is_op_req.get_true_count());
	this->ops_req.clear();
	for (auto  o : this->is_op_req.get_true_list()) {
		this->ops_req.push_back(o);
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

	auto& q = this->queue_;
	
	for (auto& train : this->trains) {
		assert(q.empty());

		idx_t o_first = train.inst->op_first;
		q.push(o_first);

		train.route_first = route_idx;

		while (!q.empty()) {
			idx_t o = q.front(); q.pop();
			auto& op = this->ops[o];

			size_t n_pred = op.inst->n_pred();
			if (n_pred == 1) {
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
		route.ops.clear();
	}

	size_t route_ops_idx = 0;

	for (auto& op : this->ops) {
		assert(op.route < this->n_routes());

		this->routes[op.route].ops.increment_size(1);
		route_ops_idx++;
	}

	this->route_ops.resize(route_ops_idx);
	assert(this->route_ops.size() == this->n_ops());

	route_ops_idx = 0;
	for (auto& route : this->routes) {
		assert(route.ops.size() > 0);
		route.ops.assign_offset(this->route_ops, route_ops_idx, true);
	}

	for (auto& op : this->ops) {
		assert(op.route < IDX_MAX);
		this->routes[op.route].ops.push_back(op.idx);
	}
}


void Preprocess::make_route_junct_level()
{
	for (auto& junct : this->juncts) {
		junct.is_route = false;
	}

	for (auto& level : this->levels) {
		level.is_route = false;
	}

	for (auto& route : this->routes) {
		idx_t o_first = route.ops[0];
		idx_t o_last = route.ops.back();

		assert(o_first < this->n_ops() && o_last < this->n_ops());

		auto& op_first = this->ops[o_first];
		auto& op_last = this->ops[o_last];

		route.junct.start = op_first.junct.start;
		route.level.start = op_first.level.start;
		
		route.junct.end = op_last.junct.end;
		route.level.end = op_last.level.end;

		this->juncts[route.junct.start].is_route = true;
		this->juncts[route.junct.end].is_route = true;

		this->levels[route.level.start].is_route = true;
		this->levels[route.level.end].is_route = true;
	}
}


void Preprocess::make_sections()
{
	for (auto& level : this->levels) {
		level.is_sect = level.is_req && level.is_route;
		
		if (level.is_sect) {
			for (auto& pred : level.pred) {
				auto& op = this->inst.ops[pred.op];
				if (op.n_succ() < level.n_succ()) {
					level.is_sect = false;
					break;
				}
			}
		}
	}

	Flag is_route_added(this->n_routes());

	this->sects.reserve(this->n_levels());
	this->sect_routes.reserve(this->n_routes());

	for (auto& train : this->trains) {
		assert(train.levels[0].is_sect);
		assert(train.levels.back().is_sect);

		train.sects.clear();

		idx_t last_level = train.level_last();
		idx_t start_level = train.level_first;
		
		while (start_level < last_level - 1) {
			assert(this->levels[start_level].is_sect);

			idx_t next_level = start_level + 1;
			Section sect = {
				.idx = (idx_t)this->sects.size(),
				.train = train.idx,
				.level = {
					.start = start_level,
					.end = next_level
				},
				.routes = {nullptr, nullptr}
			};
			
			while (!this->levels[sect.level.end].is_sect) {
				assert(sect.level.end <= last_level);
				sect.level.end++;
			}

			sect.routes.clear();
			for (idx_t l = sect.level.start; l < sect.level.end; l++) {
				auto& level = this->levels[l];
				for (auto& succ : level.succ) {
					idx_t route = this->ops[succ.op].route;
					assert(route < this->n_routes());
					if (!is_route_added[route]) {
						is_route_added += route;
						sect.routes.increment_size(1);
						this->sect_routes.push_back(route);
					}
				}
			}

			assert(sect.routes.size() >= 1);

			train.sects.increment_size(1);
			this->sects.push_back(sect);

			start_level = sect.level.end;
		}
	}

	assert(this->n_sects() <= this->n_routes());

	size_t sect_idx = 0;
	for (auto& train : this->trains) {
		train.sects.assign_offset(this->sects, sect_idx, false);
	}
	assert(sect_idx == this->n_sects());

	size_t route_idx = 0;
	for (auto& sect : this->sects) {
		sect.routes.assign_offset(this->sect_routes, route_idx, false);
	}

	this->sects.shrink_to_fit();
}


void Preprocess::make_resource_chunks()
{
	size_t n_res = this->inst.n_res;
	auto res_range = this->inst.res_range();

	size_t max_ops_count[n_res];
	size_t train_ops_count[n_res];

	for (auto r : res_range) {
		max_ops_count[r] = 0;	
	}
	
	for (auto& train : this->trains) {	
		for (auto r : res_range) {
			train_ops_count[r] = 0;	
		}

		for (auto& op : train.inst->ops) {
			for (auto& res : op.res) {
				train_ops_count[res.idx] += 1;
			}
		}

		for (auto r : res_range) {
			max_ops_count[r] = MAX(max_ops_count[r], train_ops_count[r]);	
		}
	}

	vector<vector<Op_chunk_chain>> res_ops(n_res);
	for (auto r : res_range) {
		assert(max_ops_count[r] < CNT_MAX);
		res_ops[r].reserve(max_ops_count[r]);
	}

	this->chunks.clear();
	this->chunk_ops.clear();
	this->chunks.reserve(this->inst.n_op_res());
	this->chunk_ops.reserve(this->inst.n_op_res());


	this->chunk_direct_merges = 0;
	this->chunk_parallel_merges = 0;

	set<idx_t> op_set;

	for (auto& train : this->trains) {	
		for (auto r : res_range) {
			res_ops[r].clear();
		}

		for (auto& op : train.inst->ops) {
			for (auto& res : op.res) {
				res_ops[res.idx].push_back({
					.op = op.idx,
					.res_time = DUR_MAX,
					.prev = CNT_MAX,
					.next = CNT_MAX,
				});
			}
		}

		for (auto r : res_range) {
			auto& ro = res_ops[r];
			size_t n = res_ops[r].size();

			for (size_t i = 0; i < n; i++) {
				if (!this->merge_res_op(ro, i, r, true, op_set)) {
					this->merge_res_op(ro, i, r, false, op_set);
				}
			}
			
			for (size_t i = 0; i < n; i++) {
				if (ro[i].prev < CNT_MAX) {
					continue;
				}

				Chunk chunk = {
					.idx = (idx_t)this->chunks.size(),
					.train = train.idx,
					.res = {
						.idx = r,
						.time = DUR_MAX
					},
					.ops = {nullptr, nullptr},
					.locks = {nullptr, nullptr},
					.unlocks = {nullptr, nullptr}
				};

				chunk.ops.clear();

				cnt_t j = i;
				while (j < CNT_MAX) {
					chunk.ops.increment_size(1);
					this->chunk_ops.push_back(ro[j].op);

					j = ro[j].next;
				}

				this->chunks.push_back(chunk);
			}
		}
	}

	assert(this->n_chunks() + this->chunk_direct_merges +
		this->chunk_parallel_merges == this->inst.n_op_res());

	size_t chunk_ops_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.ops.assign_offset(this->chunk_ops, chunk_ops_idx, false);

		for (auto o : chunk.ops) {
			auto& op = this->inst.ops[o];
			
			auto res = op.res.find(chunk.res);
			assert(res != nullptr);

			chunk.res.time = MIN(chunk.res.time, res->time);
		}
	}
}

bool Preprocess::merge_res_op(std::vector<Op_chunk_chain>& ro, idx_t i, idx_t r, bool match_time, set<idx_t>& op_set)
{
	auto& op = this->inst.ops[ro[i].op];

	for (int j = i - 1; (j >= 0); j--) {
		if (ro[j].next < CNT_MAX) {
			continue;
		}

		bool connect_valid = false;
		bool time_valid = true;

		cnt_t k = j;
		while (k < CNT_MAX) {
			if (!connect_valid) {
				if (op.pred.find(ro[k].op) != nullptr) {
					connect_valid = true;
				}
			}

			if (match_time && (ro[i].res_time != ro[j].res_time) 
				&& this->inst.is_op_unlock(ro[k].op, r)) {
				
				time_valid = false;
				break;
			}

			k = ro[k].prev;
			assert(k != j);
		}

		if (!time_valid) {
			continue;
		}

		if (connect_valid) {
			ro[j].next = i;
			ro[i].prev = j;

			this->chunk_direct_merges++;
			return true;
		}

		op_set.clear();
		k = j;
		while (k < CNT_MAX) {
			op_set.insert(ro[k].op);
			k = ro[k].prev;
		}

		if (!this->is_op_reachable(op_set, ro[i].op)) {
			ro[j].next = i;
			ro[i].prev = j;

			this->chunk_parallel_merges++;
			return true;
		}
	}

	return false;
}


bool Preprocess::is_op_reachable(const std::set<idx_t>& set_from, idx_t target)
{
	return this->is_op_reachable_temp<true>(set_from, target) ||
		this->is_op_reachable_temp<false>(set_from, target);
}

template<bool FWD=true>
bool Preprocess::is_op_reachable_temp(const set<idx_t>& set_from, idx_t target)
{
	auto& op_target = this->ops[target];

	this->set_.clear();
	while(!this->queue_.empty()) { this->queue_.pop(); };

	idx_t l_limit;
	if constexpr (FWD) {
		l_limit = op_target.level.end;
	}
	else {
		l_limit = op_target.level.start;
	}

	for (auto x : set_from) {
		if (x == target) {
			return true;
		}

		auto& op = this->ops[x];
		if (op_target.train == op.train) {
			this->queue_.push(x);
			this->set_.insert(x);
		}
	}

	while (!this->queue_.empty()) {
		idx_t o = this->queue_.front(); this->queue_.pop();
		

		if constexpr (FWD) {
			idx_t l = this->ops[o].level.start;
			if (l >= l_limit) {
				continue;
			}
		}
		else {
			idx_t l = this->ops[o].level.end;
			if (l <= l_limit) {
				continue;
			}
		}
		
		Array<idx_t> neig;
		if constexpr (FWD) {
			neig = this->inst.ops[o].succ;
		}
		else {
			neig = this->inst.ops[o].pred;
		}


		for (auto x : neig) {
			if (x == target) {
				return true;
			}

			if (this->set_.contains(x)) {
				continue;
			}

			this->queue_.push(x);
			this->set_.insert(x);
		}
	}

	return false;
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

void Preprocess::make_chunk_locks_unlocks()
{
	vector<Chunk_lock> curr_locks;
	vector<Chunk_unlock> curr_unlocks;

	this->chunk_locks.clear();
	this->chunk_unlocks.clear();

	for (auto& route : this->routes) {
		route.chunk_locks.clear();
		route.chunk_unlocks.clear();
	}

	for (auto& chunk : this->chunks) {
		get_chunk_locks(curr_locks, chunk);
		get_chunk_unlocks(curr_unlocks, chunk);

		bool need_locks = false;
		for (auto& lock : curr_locks | views::drop(1)) {
			if (lock != curr_locks[0]) {
				need_locks = true;
				break;
			}
		}

		chunk.locks.clear();
		if (need_locks) {
			chunk.level.start = 0;

			for (auto& lock : curr_locks) {
				chunk.level.start = MAX(chunk.level.start, lock.level);
				
				chunk.locks.increment_size(1);
				this->routes[lock.route].chunk_locks.increment_size(1);
				this->chunk_locks.push_back(lock);
			}
		}
		else {
			chunk.level.start = curr_locks[0].level;
		}

		bool need_unlocks = false;
		for (auto& unlock : curr_unlocks | views::drop(1)) {
			if (unlock != curr_unlocks[0]) {
				need_unlocks = true;
				break;
			}
		}

		chunk.unlocks.clear();
		if (need_unlocks) {
			chunk.level.end = IDX_MAX;
			chunk.res.time = DUR_MAX;

			for (auto& unlock : curr_unlocks) {
				chunk.level.end = MIN(chunk.level.end, unlock.level);
				chunk.res.time = MIN(chunk.res.time, unlock.time);

				chunk.unlocks.increment_size(1);
				this->routes[unlock.route].chunk_unlocks.increment_size(1);
				this->chunk_unlocks.push_back(unlock);
			}
		}
		else {
			chunk.level.end = curr_unlocks[0].level;
			chunk.res.time = curr_unlocks[0].time;
		}
	}

	size_t lock_idx = 0;
	size_t unlock_idx = 0;

	for (auto& chunk : this->chunks) {
		chunk.locks.assign_offset(this->chunk_locks, lock_idx, false);
		chunk.unlocks.assign_offset(this->chunk_unlocks, unlock_idx, false);
	}

	lock_idx = 0;
	unlock_idx = 0;

	this->route_locks.resize(this->chunk_locks.size());
	this->route_unlocks.resize(this->chunk_unlocks.size());

	for (auto& route : this->routes) {
		route.chunk_locks.assign_offset(this->route_locks, lock_idx, true);
		route.chunk_unlocks.assign_offset(this->route_unlocks, unlock_idx, true);
	}

	for (auto& lock : this->chunk_locks) {
		this->routes[lock.route].chunk_locks.push_back(lock);
	}

	for (auto& unlock : this->chunk_unlocks) {
		this->routes[unlock.route].chunk_unlocks.push_back(unlock);
	}
}

void Preprocess::get_chunk_locks(std::vector<Chunk_lock>& locks, const Chunk& chunk)
{
	locks.clear();

	for (auto o : chunk.ops) {
		auto& op = this->ops[o];
		auto op_res = op.inst->res.find(chunk.res.idx);
		assert(op_res != nullptr);

		if (op.inst->n_pred() == 0) {
			locks.push_back({
				.chunk = chunk.idx,
				.route = op.route,
				.level = op.level.start,
			});
		}

		for (auto p : op.inst->pred) {
			auto& op_pred = this->ops[p];
			if (op_pred.inst->res.find(chunk.res.idx) == nullptr) {
				locks.push_back({
					.chunk = chunk.idx,
					.route = op_pred.route,
					.level = op.level.start,
				});
			}
		}
	}
}


void Preprocess::get_chunk_unlocks(std::vector<Chunk_unlock>& unlocks, const Chunk& chunk)
{
	unlocks.clear();

	for (auto o : chunk.ops) {
		auto& op = this->ops[o];
		auto op_res = op.inst->res.find(chunk.res.idx);
		assert(op_res != nullptr);

		if (op.inst->n_succ() == 0) {
			unlocks.push_back({
				.chunk = chunk.idx,
				.route = op.route,
				.level = op.level.end,
				.time = op_res->time,
			});
		}

		for (auto s : op.inst->succ) {
			auto& op_succ = this->ops[s];
			if (op_succ.inst->res.find(chunk.res) == nullptr) {
				unlocks.push_back({
					.chunk = chunk.idx,
					.route = op_succ.route,
					.level = op.level.end,
					.time = op_res->time,
				});
			}
		}
	}
}


void Preprocess::make_level_bounds()
{

}


