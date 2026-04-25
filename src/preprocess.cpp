#include "preprocess.hpp"

#include <cstdio>
#include <iostream>
#include <set>
#include <tuple>

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
	this->make_section_min_dur();
	cout << "  sects:   " << this->n_sects() << endl;

	this->make_resource_chunks();
	this->assign_op_chunks();
	
	cout << "  chunks:  " << this->n_chunks() << endl;

	if (this->chunk_direct_merges > 0) {
		cout << "    direct merges:   " << this->chunk_direct_merges << endl;
	}

	if (this->chunk_parallel_merges > 0) {
		cout << "    parralel merges: " << this->chunk_parallel_merges << endl;
	}

	if (verify) {
		this->verify_chunks();
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

		if (level.is_sect) {
			assert(level.n_juncts() == 1);
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

			size_t n_routes = 0;
			for (idx_t l = sect.level.start; l < sect.level.end; l++) {
				auto& level = this->levels[l];
				for (auto& succ : level.succ) {
					idx_t route = this->ops[succ.op].route;
					assert(route < this->n_routes());
					if (!is_route_added[route]) {
						is_route_added += route;
						this->sect_routes.push_back(route);
						n_routes++;
					}
				}
			}

			assert(n_routes > 0);
			sect.routes.set_size(n_routes);
			sect.is_single_route = (n_routes == 1);

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


void Preprocess::make_section_min_dur()
{
	tim_t dist[this->n_ops()];
	for (auto o : this->ops_range()) {
		dist[o] = TIM_MAX;
	}

	for (auto& sect : this->sects) {
		sect.min_dur = 0;
		if (sect.is_single_route) {
			for (auto o : this->routes[sect.routes[0]].ops) {
				sect.min_dur += this->inst.ops[o];
			}
			continue;
		}

		auto& pq = this->prio_queue;
		while (!pq.empty()) { pq.pop(); }

		auto& level_start = this->levels[sect.level.start];
		idx_t l_end = sect.level.end;

		for (auto succ : level_start.succ) {
			dist[succ.op] = 0;
			pq.push({0, succ.op});
		}

		bool found = false;
		while (!pq.empty()) {
			auto curr = pq.top(); pq.pop();

			if (dist[curr.second] < curr.first) {
				continue;
			}

			auto& op = this->ops[curr.second];
			if (op.level.start == l_end) {
				sect.min_dur = dist[op.idx];
				found = true;
				break;
			}

			tim_t succ_dist = dist[op.idx] + op.inst->dur;
			for (auto s : op.inst->succ) {
				if (dist[s] > succ_dist) {
					pq.push({succ_dist, s});
					dist[s] = succ_dist;
				}
			}
		}
		assert(found);
	}
}


void Preprocess::make_resource_chunks()
{
	size_t n_res = this->inst.n_res;
	size_t n_trains = this->n_trains();
	auto res_range = this->inst.res_range();

	size_t total_ops_count = 0;
	size_t max_ops_count[n_res];
	size_t train_ops_count[n_res];

	for (auto r : res_range) {
		max_ops_count[r] = 0;	
	}
	
	this->train_chunks.resize(n_trains*n_res);
	for (auto& train : this->trains) {
		train.chunks = &(this->train_chunks[train.idx*n_res]);

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

	vector<vector<idx_t>> res_ops(n_res);
	for (auto r : res_range) {
		assert(max_ops_count[r] < CNT_MAX);
		res_ops[r].reserve(max_ops_count[r]);
		total_ops_count = MAX(total_ops_count, max_ops_count[r]);
	}

	this->chunks.reserve(this->inst.n_op_res());
	this->chunk_ops.reserve(this->inst.n_op_res());

	this->chunk_direct_merges = 0;
	this->chunk_parallel_merges = 0;

	vector<vector<idx_t>> chunk_sets = {};

	Disjoint_set disj_set(total_ops_count);


	size_t chunk_idx = 0;
	for (auto& train : this->trains) {	
		for (auto r : res_range) {
			res_ops[r].clear();
		}

		for (auto& op : train.inst->ops) {

			for (auto& res : op.res) {
				res_ops[res.idx].push_back({op.idx});
			}
		}


		for (auto r : res_range) {
			auto& ro = res_ops[r];
			size_t n_ops = ro.size();

			disj_set.resize(n_ops);
			disj_set.reset();

			for (size_t i = 0; i < n_ops; i++) {
				idx_t a = ro[i];
				auto& op = this->ops[a];
				for (size_t j = i + 1; j < n_ops; j++) {
					idx_t b = ro[j];

					if (op.inst->pred.find(b)) {
						disj_set.union_set(i, j);
					}

					if (op.inst->succ.find(b)) {
						disj_set.union_set(i, j);
					}
				}
			}

			auto& res = disj_set.get_result();
			this->chunk_direct_merges += disj_set.n_items - disj_set.n_sets;

			chunk_sets.clear();
			chunk_sets.resize(disj_set.n_sets);

			for (size_t i = 0; i < n_ops; i++) {
				chunk_sets[res[i]].push_back(ro[i]);
			}

			for (size_t i = 0; i < chunk_sets.size();) {
				size_t j;
				for (j = i + 1; j < chunk_sets.size(); j++) {
					if (!ops_reachable(chunk_sets[i], chunk_sets[j]) &&
						!ops_reachable(chunk_sets[j], chunk_sets[i])) {
					
						break;			
					}
				}

				if (j == chunk_sets.size()) {
					i++;
				}
				else {
					chunk_sets[i].insert(chunk_sets[i].end(), 
						chunk_sets[j].begin(), chunk_sets[j].end());

					chunk_sets[j] = chunk_sets.back();
					chunk_sets.pop_back();

					this->chunk_parallel_merges++;
				}
			}

			for (auto& x : chunk_sets) {
				assert(!x.empty());

				Chunk chunk = {
					.idx = (idx_t)chunk_idx++,
					.train = train.idx,
					.res = r,
					.state = {{IDX_MAX, IDX_MAX}, 0},
					.ops = {nullptr, nullptr}
				};

				sort(x.begin(), x.end());

				chunk.ops.set_size(x.size());
				for (auto o : x) {
					this->chunk_ops.push_back(o);
				}

				train.chunks[r].increment_size(1);
				this->chunks.push_back(chunk);
			}
		}
	}

	assert(chunk_idx < IDX_MAX);
	assert(chunk_idx == this->n_chunks());
	
	this->chunks.shrink_to_fit();

	assert(chunk_idx + this->chunk_direct_merges +
		this->chunk_parallel_merges == this->inst.n_op_res());
	

	chunk_idx = 0;
	for (auto& train : this->trains) {
		for (auto r : res_range) {
			train.chunks[r].assign_offset(this->chunks, chunk_idx, false);
		}
	}
	assert(chunk_idx == this->n_chunks());

	size_t chunk_ops_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.ops.assign_offset(this->chunk_ops, chunk_ops_idx, false);

		for (auto o : chunk.ops) {
			auto& op = this->ops[o];
			
			auto res = op.inst->res.find(chunk.res);
			assert(res != nullptr);

			chunk.state.level.start = MAX(chunk.state.level.start, op.level.start);
			chunk.state.level.end = MIN(chunk.state.level.end, op.level.end);
			chunk.state.rel_time = MIN(chunk.state.rel_time, res->dur);
		}
	}

	this->res_chunks.resize(n_res);
	this->res_chunks_data.resize(chunk_idx);

	for (auto r : res_range) {
		this->res_chunks[r].set_size(0);
	}

	for (auto& chunk : this->chunks) {
		this->res_chunks[chunk.res].increment_size(1);
	}

	chunk_idx = 0;
	for (auto r : res_range) {
		this->res_chunks[r].assign_offset(this->res_chunks_data, chunk_idx, true);
	}
	assert(chunk_idx == this->n_chunks());

	for (auto& chunk : this->chunks) {
		this->res_chunks[chunk.res].push_back(chunk.idx);
	}
}


void Preprocess::assign_op_chunks()
{
	for (auto op : this->ops) {
		op.chunks.clear();
	}

	for (auto& chunk : chunks) {
		for (auto& o : chunk.ops) {
			this->ops[o].chunks.increment_size(1);
		}
	}

	this->op_chunks.resize(this->chunk_ops.size());
	size_t op_chunks_idx = 0;
	for (auto& op : this->ops) {
		op.chunks.assign_offset(this->op_chunks, op_chunks_idx, true);
	}

	assert(op_chunks_idx == this->op_chunks.size());
	for (auto& chunk : chunks) {
		for (auto& o : chunk.ops) {
			this->ops[o].chunks.push_back(chunk.idx);
		}
	}
}


void Preprocess::verify_chunks()
{
	for (auto& chunk : this->chunks) {
		assert(this->res_chunks[chunk.res].find_asc(chunk.idx) != nullptr);
		assert(this->trains[chunk.train].chunks[chunk.res].find_asc(chunk.idx) != nullptr);

		for (auto o : chunk.ops) {
			auto& op = this->ops[o];
			assert(op.chunks.find_asc(chunk.idx) != nullptr);
			assert(op.inst->res.find_asc(chunk.res) != nullptr);

			for (auto& x : op.chunks) {
				if (x == chunk.idx) { continue; }

				assert(this->chunks[x].res != chunk.res);
			}

			for (auto p : op.inst->pred) {
				for (auto& x : this->ops[p].chunks) {
					if (x == chunk.idx) { continue; }

					assert(this->chunks[x].res != chunk.res);
				}
			}


			for (auto s : op.inst->pred) {
				for (auto& x : this->ops[s].chunks) {
					if (x == chunk.idx) { continue; }

					assert(this->chunks[x].res != chunk.res);
				}
			}
		}
	}
}


void Preprocess::make_objs()
{
	this->objs.reserve(this->inst.objs.size());


	for (auto& obj_i : this->inst.objs) {
		auto op = this->ops[obj_i.op];

		bool is_bin = obj_i.increment > 0;
		assert(!is_bin || (obj_i.coeff == 0));

		Obj obj = {
			.idx = (idx_t)this->objs.size(),
			.train = op.train,
			.level = op.level.start,
			.route = op.route,
			.is_bin = is_bin,
			.coeff = (is_bin ? obj_i.increment : obj_i.coeff),
			.threshold = obj_i.threshold
		};

		this->objs.push_back(obj);
	}
}


bool Preprocess::ops_reachable(const std::vector<idx_t>& vec_from, const std::vector<idx_t>& vec_to)
{
	auto& visited = this->set_;
	auto& q = this->queue_;

	visited.clear();

	idx_t l_max = 0;

	for (auto o : vec_to) {
		l_max = MAX(l_max, this->ops[o].level.start);
	}

	for (auto o : vec_from) {
		q.push(o);
		visited.insert(o);
	}


	while (!q.empty()) {
		idx_t o = q.front(); q.pop();
		auto& op = this->ops[o];

		if (op.level.start > l_max) { continue; }

		for (auto s : op.inst->succ) {
			auto ret = visited.insert(s);
			if (ret.second) {
				q.push(s);
			}
		}
	}

	for (auto x : vec_to) {
		if (visited.contains(x)) {
			return true;
		}
	}

	return false;
}


void Preprocess::get_link_set(set<idx_t>& link_set, const Chunk& chunk) const
{
	link_set.clear();
	for (auto o : chunk.ops) {
		auto& op = this->ops[o];
		for (auto& x : op.chunks) {
			if (x == chunk.idx) { continue; }

			assert(this->chunks[x].res != chunk.res);
			link_set.insert(x);
		}

		for (auto s : op.inst->succ) {
			for (auto& x : this->ops[s].chunks) {
				if (x == chunk.idx) { continue; }

				assert(this->chunks[x].res != chunk.res);
				link_set.insert(x);
			}
		}
	}
}
