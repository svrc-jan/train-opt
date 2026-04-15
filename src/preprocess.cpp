#include "preprocess.hpp"

#include <cstdio>
#include <iostream>
#include "utils/disjoint_set.hpp"
#include "utils/stl_print.hpp"


using namespace std;


Preprocess::Preprocess(const Instance& inst) : inst(inst)
{
	this->trains.resize(this->inst.n_trains(), Train());
	this->make_junctions();
	this->make_junctions_bounds();
	this->make_levels();
}

void Preprocess::make_junctions()
{
	size_t n_ops = this->inst.n_ops();
	const size_t n_trains = this->inst.n_trains();

	this->op_junct.resize(n_ops, {IDX_MAX, IDX_MAX});
	this->trains.resize(n_trains, Train());

	int junct_idx = 0;

	for (size_t t = 0; t < n_trains; t++) {

		auto& inst_train = this->inst.trains[t];
		auto& train = this->trains[t];

		Disjoint_set disj_set(inst_train.ops.size());

		for (auto& op : inst_train.ops) {
			for (auto it_a = op.succ.begin(); it_a < op.succ.end(); it_a++) {
				for (auto it_b = it_a + 1; it_b < op.succ.end(); it_b++) {
					int a = *it_a - inst_train.op_first;
					int b = *it_b - inst_train.op_first;
					disj_set.union_set(a, b);
				}
			}
		}
		
		train.junct_first = junct_idx;

		int diff = 0;
		auto set_idx = disj_set.get_result();
		
		for (auto& op : inst_train.ops) {
			if (op.is_leading() || op.is_trailing()) {
				diff += 1;
				continue;
			}

			this->op_junct[op.idx].start = set_idx[op.idx - inst_train.op_first] + 
				train.junct_first - diff;
		}

		train.juncts.set_size(disj_set.n_sets + 1 - diff);

		if (inst_train.has_trailing) {
			for (idx_t p : this->inst.ops[inst_train.op_last()].pred) {
				this->op_junct[p].end = train.junct_last();
			}
		}
		else {
			this->op_junct[inst_train.op_last()].end = train.junct_last();
		}		

		junct_idx += train.juncts.size();
	}

	for (auto& op : this->inst.ops) {
		auto& j_op = this->op_junct[op.idx];
		
		if (op.is_leading() || op.is_trailing()) {
			assert(j_op.start == IDX_MAX);
			continue;
		}

		assert(j_op.start < junct_idx);

		for (int p : op.pred) {
			if (this->inst.ops[p].is_leading()) {
				continue;
			}

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

		if (op.is_leading() || op.is_trailing()) {
			assert(j_op.start == IDX_MAX);
			continue;
		}

		assert(j_op.end < junct_idx);

		this->juncts[j_op.start].succ.increment_size(1);
		this->juncts[j_op.end].pred.increment_size(1);

		n_ops += 1;
	}

	this->junct_succ.resize(n_ops);
	this->junct_pred.resize(n_ops);

	size_t succ_idx = 0;
	size_t pred_idx = 0;

	for (auto& junct : this->juncts) {
		junct.succ.assign_offset(this->junct_succ, succ_idx, true);
		junct.pred.assign_offset(this->junct_pred, pred_idx, true);
	}

	for (auto& op : this->inst.ops) {
		if (op.is_leading() || op.is_trailing()) {
			continue;
		}

		auto& j_op = this->op_junct[op.idx];

		this->juncts[j_op.start].succ.push_back({j_op.end, op.idx});
		this->juncts[j_op.end].pred.push_back({j_op.start, op.idx});
	}
}


void Preprocess::make_junctions_bounds()
{
	for (auto& inst_train : this->inst.trains) {
		auto& train = this->trains[inst_train.idx];

		auto& op_last = this->inst.ops[inst_train.op_last()];
		auto& junct_last = this->juncts[train.junct_last()];
		
		junct_last.time_lb = op_last.start_lb;
		junct_last.time_ub = op_last.start_ub;

		if (inst_train.has_trailing == 0) {
			junct_last.time_lb += op_last.dur;
			junct_last.time_ub += op_last.dur;
		}
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


void Preprocess::make_levels()
{
	
}




