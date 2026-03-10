#pragma once

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"


class Path_select
{
public:
	const Instance& inst;
	const Preprocess& prepr;

	Path_select(const Preprocess& prepr, const GRBEnv& grb_env);
	~Path_select();

	void select_paths(std::vector<std::vector<int>>& paths,
		std::vector<int>& junct_time);

	void select_paths(std::vector<std::vector<int>>& paths);

private:
	struct Res_overlap;
	struct Res_interval;

	const GRBEnv& grb_env;
	GRBModel* model = nullptr;

	GRBVar* op_var = nullptr;
	std::vector<GRBVar> res_var;
	std::vector<GRBConstr> res_cons;

	void get_res_overlaps(std::vector<Res_overlap>& overlaps,
		const std::vector<int>& junct_time);

	void add_overlaps_to_model(const std::vector<Res_overlap>& overlaps);
	void propagate_junct_time(std::vector<int>& junct_time);

	void extract_paths_from_sol(std::vector<std::vector<int>>& paths);
};


struct Path_select::Res_overlap
{
	int op1;
	int op2;
	int size;

	Res_overlap(int op1_, int op2_, int size_) 
		: size(size_)
	{
		this->op1 = (op1_ < op2_) ? op1_ : op2_;
		this->op2 = (op1_ < op2_) ? op2_ : op1_;
		this->size = size_;
	}

	bool operator<(const Res_overlap& other) 
	{
		return (this->op1 < other.op1) || 
			((this->op1 == other.op1) && (this->op2 < other.op2));
	}
};


struct Path_select::Res_interval
{
	int train;
	int op;
	int start;
	int end;

	Res_interval(int train_, int op_, int start_, int end_)
		: train(train_), op(op_), start(start_), end(end_) {}

	bool operator<(const Res_interval& other)
	{
		return (this->start < other.start) || 
			((this->start == other.start) && (this->end < other.end));
	}

	void print(std::ostream& os) const;
	friend std::ostream& operator<<(std::ostream& os, const Res_interval& ri)
	{
		ri.print(os);
		return os;
	}
};
