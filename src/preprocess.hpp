#pragma once

#include "utils/interval.hpp"
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

	std::vector<Interval<idx_t>> op_junct = {};
	std::vector<Interval<idx_t>> op_level = {};
	std::vector<Junction> juncts = {};
	std::vector<Train> trains = {};
	std::vector<Level> levels = {};
	
	Preprocess(const Instance& inst);
	
	METHOD_N(juncts)
	METHOD_N(levels)
	
private:
	std::vector<Junct_op> junct_succ = {};
	std::vector<Junct_op> junct_pred = {};

	void make_junctions();
	void make_junctions_bounds();

	void make_levels();
};

struct Preprocess::Junction
{
	tim_t time_lb = 0;
	tim_t time_ub = TIME_MAX;

	Array<Junct_op> succ;
	Array<Junct_op> pred;
};


struct Preprocess::Level
{
	tim_t time_lb = 0;
	tim_t time_ub = TIME_MAX;
	Array<uint16_t> juncts;
};


struct Preprocess::Train
{
	idx_t junct_first = IDX_MAX;
	idx_t level_first = IDX_MAX;

	Array<Junction> juncts; 
	Array<Level> levels;
	
	METHOD_AFTER(junct)
	METHOD_LAST(junct)
	METHOD_AFTER(level)
	METHOD_LAST(level)
};


struct Preprocess::Junct_op
{
	idx_t junct = IDX_MAX;
	idx_t op = IDX_MAX;
};