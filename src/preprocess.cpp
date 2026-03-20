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

	this->ops.resize(n_ops, Op());
	this->trains.resize(n_trains, Train());

	int junct_idx = 0;

	for (size_t t = 0; t < n_trains; t++) {

		auto& inst_train = this->inst.trains[t];
		auto& train = this->trains[t];

		Disjoint_set disj_set(inst_train.ops.size);

		for (auto& op : inst_train.ops) {
			for (size_t i = 0; i < op.succ.size; i++) {
				for (size_t j = i + 1; j < op.succ.size; j++) {
					int a = op.succ[i] - inst_train.op_start;
					int b = op.succ[j] - inst_train.op_start;
					disj_set.union_set(a, b);
				}
			}
		}
		
		train.junct_start = junct_idx;
		train.juncts.size = disj_set.n_sets + 1;

		int diff = 0;
		auto set_idx = disj_set.get_result();
		
		for (auto& op : inst_train.ops) {
			if (op.res.size == 0 && (op.pred.size == 0 || op.succ.size == 0)) {
				diff += 1;
				continue;
			}

			this->ops[op.idx].junct_start = set_idx[op.idx - inst_train.op_start] + 
				train.junct_start - diff;
		}

		train.juncts.size -= diff;

		if (inst_train.has_trailing) {
			for (idx_t p : this->inst.ops[inst_train.op_last()].pred) {
				this->ops[p].junct_end = train.junct_last();
			}
		}
		else {
			this->ops[inst_train.op_last()].junct_end = train.junct_last();
		}		

		junct_idx += train.juncts.size;
	}


	for (auto& inst_op : this->inst.ops) {
		auto& op = this->ops[inst_op.idx];
		
		if (inst_op.is_leading() || inst_op.is_trailing()) {
			assert(op.junct_start == IDX_MAX);
			continue;
		}

		assert(op.junct_start < junct_idx);

		for (int p : inst_op.pred) {
			if (this->inst.ops[p].is_leading()) {
				continue;
			}

			auto& op_p = this->ops[p];
			if (op_p.junct_end == UINT16_MAX) {
				op_p.junct_end = op.junct_start;
			} 
			else {
				assert(op_p.junct_end == op.junct_start);
			}
		}
	}


	this->juncts.resize(junct_idx, Junction());

	for (auto& inst_op : this->inst.ops) {
		auto& op = this->ops[inst_op.idx];

		if (inst_op.is_leading() || inst_op.is_trailing()) {
			assert(op.junct_end == IDX_MAX);
			continue;
		}

		assert(op.junct_end < junct_idx);

		this->juncts[op.junct_start].succ.size += 1;
		this->juncts[op.junct_end].pred.size += 1;

		n_ops += 1;
	}

	this->junct_succ.resize(n_ops);
	this->junct_pred.resize(n_ops);

	size_t succ_idx = 0;
	size_t pred_idx = 0;

	for (auto& junct : this->juncts) {
		junct.succ.assign_ptr(this->junct_succ, succ_idx);
		junct.pred.assign_ptr(this->junct_pred, pred_idx);

		junct.succ.size = 0;
		junct.pred.size = 0;
	}

	for (auto& inst_op : this->inst.ops) {
		if (inst_op.is_leading() || inst_op.is_trailing()) {
			continue;
		}

		idx_t o = inst_op.idx;
		auto& op = this->ops[o];

		this->juncts[op.junct_start].succ.push_back({op.junct_end, (uint16_t)o});
		this->juncts[op.junct_end].pred.push_back({op.junct_start, (uint16_t)o});
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
		if (junct.succ.size > 0) {
			junct.time_lb = UINT32_MAX;
			junct.time_ub = 0;

			for (auto& succ : junct.succ) {
				auto& op = this->inst.ops[succ.op];
				junct.time_lb = min(junct.time_lb, op.start_lb);
				junct.time_ub = max(junct.time_ub, op.start_ub);
			}
		}
	}
}


void Preprocess::make_levels()
{

}




