#pragma once

#include "instance.hpp"

class Preprocess
{
public:
	struct Op;
	struct Junction;
	struct Level;
	struct Train;

	struct Junct_op;
	
	const Instance& inst;

	std::vector<Op> ops = {};
	std::vector<Junction> juncts = {};
	std::vector<Train> trains = {};
	std::vector<Level> levels = {};
	
	Preprocess(const Instance& inst);
	
	inline size_t n_juncts() const { return this->juncts.size(); }
	inline size_t n_levels() const { return this->levels.size(); }
	
private:
	std::vector<Junct_op> junct_succ = {};
	std::vector<Junct_op> junct_pred = {};

	void make_junctions();
	void make_junctions_bounds();

	void make_levels();
};


struct Preprocess::Op
{
	idx_t junct_start = IDX_MAX;
	idx_t junct_end   = IDX_MAX;
	idx_t level_start = IDX_MAX;
	idx_t level_end   = IDX_MAX;
};

struct Preprocess::Junction
{
	tim_t time_lb = 0;
	tim_t time_ub = TIME_MAX;

	Array<Junct_op> succ  = {nullptr, 0};
	Array<Junct_op> pred = {nullptr, 0};
};


struct Preprocess::Level
{
	tim_t time_lb = 0;
	tim_t time_ub = TIME_MAX;
	Array<uint16_t> juncts = {nullptr, 0};
};


struct Preprocess::Train
{
	idx_t junct_start = IDX_MAX;
	idx_t level_start = IDX_MAX;

	Array<Junction> juncts = {nullptr, 0}; 
	Array<Level> levels = {nullptr, 0};
	
	inline idx_t junct_last() const { return this->junct_start + this->juncts.size - 1; }
	inline idx_t junct_end() const { return this->junct_start + this->juncts.size; }

	inline idx_t level_last() const { return this->level_start + this->levels.size - 1; }
	inline idx_t level_end() const { return this->level_start + this->levels.size; }
};


struct Preprocess::Junct_op
{
	idx_t junct = IDX_MAX;
	idx_t op = IDX_MAX;
};