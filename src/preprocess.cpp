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

	this->make_count();
	this->make_level_res();
	this->make_train_res();
	this->make_global_res();
}


Preprocess::~Preprocess()
{

}


void Preprocess::make_junctions()
{
	size_t n_ops = this->inst.n_ops();
	const size_t n_trains = this->inst.n_trains();

	this->op_junct.resize(n_ops, {IDX_MAX, IDX_MAX});
	this->trains.resize(n_trains, Train());

	size_t junct_idx = 0;

	for (size_t t = 0; t < n_trains; t++) {

		auto& inst_train = this->inst.trains[t];
		auto& train = this->trains[t];
		train.idx = t;

		Disjoint_set disj_set(inst_train.ops.size());

		const idx_t o_first = inst_train.op_first;

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
		
		for (auto& op : inst_train.ops) {
			this->op_junct[op.idx].start = set_idx[op.idx - o_first] + junct_idx;
		}

		train.juncts.set_size(disj_set.n_sets + 1);
		this->op_junct[inst_train.op_last()].end = train.junct_last();	

		junct_idx += train.juncts.size();
	}

	for (auto& op : this->inst.ops) {
		auto& j_op = this->op_junct[op.idx];
		assert(j_op.start < junct_idx);

		for (int p : op.pred) {
			auto& j_pred = this->op_junct[p];
			if (j_pred.end == IDX_MAX) {
				j_pred.end = j_op.start;
			} 
			else {
				assert(j_pred.end == j_op.start);
			}
		}
	}


	this->juncts.resize(junct_idx, Junction());

	for (auto& op : this->inst.ops) {
		auto& j_op = this->op_junct[op.idx];
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

	for (auto& op : this->inst.ops) {
		auto& j_op = this->op_junct[op.idx];

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
			assert(this->op_junct[s.op].start == j);
		}

		for (auto& p : junct.pred) {
			assert(this->op_junct[p.op].end == j);
		}
	}

	for (auto o : this->inst.ops_range()) {
		auto& j = this->op_junct[o];
		assert(j.start < this->n_juncts() && j.end < this->n_juncts());

		assert(this->juncts[j.start].succ.find(Idx_op(j.end, o)) != nullptr);
		assert(this->juncts[j.end].pred.find(Idx_op(j.start, o)) != nullptr);
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

	this->op_level.resize(this->inst.n_ops(), {IDX_MAX, IDX_MAX});

	for (idx_t o = 0; o < this->inst.n_ops(); o++) {
		idx_t l_start = this->juncts[this->op_junct[o].start].level;
		idx_t l_end = this->juncts[this->op_junct[o].end].level;

		this->op_level[o].start = l_start;
		this->op_level[o].end = l_end;

		this->levels[l_start].succ.push_back({l_end, o});
		this->levels[l_end].pred.push_back({l_start, o});
	}
}


void Preprocess::verify_levels() const
{
	for (auto l : this->levels_range()) {
		auto& level = this->levels[l];
		assert(level.idx == l);

		for (auto& s : level.succ) {
			assert(this->op_level[s.op].start == l);
		}

		for (auto& p : level.pred) {
			assert(this->op_level[p.op].end == l);
		}
	}

	for (auto o : this->inst.ops_range()) {
		auto& l = this->op_level[o];
		assert(l.start < this->n_levels() && l.end < this->n_levels());

		assert(this->levels[l.start].succ.find(Idx_op(l.end, o)) != nullptr);
		assert(this->levels[l.end].pred.find(Idx_op(l.start, o)) != nullptr);
	}


	for (auto& train : this->trains) {
		for (auto& levels : train.levels) {
			assert(levels.train == train.idx);
		}
	}
}


void Preprocess::make_junctions_bounds()
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


void Preprocess::make_count()
{
	size_t n_levels = this->n_levels();
	size_t n_res = this->inst.n_res();

	this->op_count.resize(n_levels, 0);
	this->res_count.resize(n_levels, nullptr);
	this->res_count_data.resize(n_levels*n_res, 0);

	for (auto l : this->levels_range()) {
		this->res_count[l] = &this->res_count_data[l*n_res];
	}

	for (auto& op : this->inst.ops) {
		for (auto l : this->op_level[op.idx].range()) {
			
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
	size_t req_idx = 0;
	size_t opt_idx = 0;

	for (auto& level : this->levels) {
		level.req_res.set_size(0);
		level.opt_res.set_size(0);

		cnt_t o_cnt = this->op_count[level.idx];
		for (auto r : this->inst.res_range()) {
			cnt_t r_cnt = this->res_count[level.idx][r];
			
			if (r_cnt == 0) {
				// nothing
			}
			else if (r_cnt == o_cnt) {
				level.req_res.increment_size(1);
				req_idx += 1;
			}
			else {
				assert(r_cnt < o_cnt);
				level.opt_res.increment_size(1);
				opt_idx += 1;
			}
		}
	}

	this->level_req_res.resize(req_idx);
	this->level_opt_res.resize(opt_idx);

	req_idx = 0;
	opt_idx = 0;

	for (auto& level : this->levels) {
		level.req_res.assign_offset(this->level_req_res, req_idx, true);
		level.opt_res.assign_offset(this->level_opt_res, opt_idx, true);


		cnt_t o_cnt = this->op_count[level.idx];
		for (auto r : this->inst.res_range()) {
			cnt_t r_cnt = this->res_count[level.idx][r];
			
			if (r_cnt == 0) {
				// nothing
			}
			else if (r_cnt == o_cnt) {
				level.req_res.push_back(r);
			}
			else {
				level.opt_res.push_back(r);
			}
		}
	}
}


void Preprocess::make_train_res()
{
	size_t n_res = this->inst.n_res();
	size_t n_trains = this->n_trains();
	
	this->train_res_state.resize(n_trains*n_res, RES_MISS);

	for (auto& train : this->trains) {
		train.res_state.set_begin(&this->train_res_state[train.idx*n_res]);
		train.res_state.set_size(n_res);

		for (auto& level : train.levels) {
			for (auto& r : level.req_res) {
				train.res_state[r] |= RES_REQ;
			}
			for (auto& r : level.opt_res) {
				train.res_state[r] |= RES_OPT;
			}
		}
		
		for (auto& x : train.res_state) {
			if (x == RES_SPLIT) {
				x = RES_REQ;
			}
		}
	}
}


void Preprocess::make_global_res()
{
	size_t n_res = this->inst.n_res();
	this->global_res_state.resize(n_res, RES_MISS);

	for (auto& train : this->trains) {
		for (auto r : this->inst.res_range()) {
			this->global_res_state[r] |= train.res_state[r];
		}
	}
}
