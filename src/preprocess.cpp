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
	this->make_levels();
}

void Preprocess::make_junctions()
{
	const int n_ops = this->inst.n_ops();
	const int n_trains = this->inst.n_trains();

	this->ops.resize(n_ops, Op());
	this->trains.resize(n_trains, Train());

	int n_juncts = 0;

	for (int t = 0; t < n_trains; t++) {
		auto& inst_train = this->inst.trains[t];
		auto& train = this->trains[t];

		const int n_train_ops = inst_train.ops.size;
		const int op_offset = inst_train.op_start;

		Disjoint_set disj_set(n_train_ops);
		for (auto& op : inst_train.ops) {
			for (int i = 0; i < op.succ.size; i++) {
				for (int j = i + 1; j < op.succ.size; j++) {
					int a = op.succ[i] - op_offset;
					int b = op.succ[j] - op_offset;
					disj_set.union_set(a, b);
				}
			}
		}
		
		train.junct_start = n_juncts;
		train.juncts.size = disj_set.n_sets + 1;
		n_juncts += train.juncts.size;

		const auto succ_set = disj_set.get_result();
		for (int i = 0; i < n_train_ops; i++) {
			this->ops[i + op_offset].junct_start = succ_set[i] + train.junct_start;
		}

		this->ops[inst_train.op_last()].junct_end = train.junct_last();
	}

	for (int o = 0; o < n_ops; o++) {
		auto& op = this->ops[o];
		assert(op.junct_start >= 0 && op.junct_start < n_juncts);

		for (int p : this->inst.ops[o].pred) {
			auto& op_p = this->ops[p];
			if (op_p.junct_end == -1) {
				op_p.junct_end = op.junct_start;
			} 
			else {
				assert(op_p.junct_end == op.junct_start);
			}
		}
	}

	this->juncts.resize(n_juncts, Junction());

	for (int o = 0; o < n_ops; o++) {
		auto& op = this->ops[o];
		assert(op.junct_end >= 0 && op.junct_end < n_juncts);

		this->juncts[op.junct_start].ops_out.size += 1;
		this->juncts[op.junct_end].ops_in.size += 1;
	}

	this->junct_ops_in.resize(n_ops);
	this->junct_ops_out.resize(n_ops);

	int in_idx = 0;
	int out_idx = 0;

	for (auto& junct : this->juncts) {
		junct.ops_in.assign_ptr(this->junct_ops_in, in_idx);
		junct.ops_out.assign_ptr(this->junct_ops_out, out_idx);

		junct.ops_in.size = 0;
		junct.ops_out.size = 0;
	}

	for (int o = 0; o < n_ops; o++) {
		auto& op = this->ops[o];

		this->juncts[op.junct_start].ops_out.push_back(o);
		this->juncts[op.junct_end].ops_in.push_back(o);
	}
}


void Preprocess::make_levels()
{

}




