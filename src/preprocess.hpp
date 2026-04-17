#pragma once

#include "utils/flag.hpp"
#include "instance.hpp"


class Preprocess
{
public:
	struct Op;
	struct Junction;
	struct Level;
	struct Train;

	struct Idx_op;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef uint8_t cnt_t;

	static const idx_t IDX_MAX = Instance::IDX_MAX;
	static const dur_t DUR_MAX = Instance::DUR_MAX;
	static const tim_t TIM_MAX = Instance::TIM_MAX;
	static const cnt_t CNT_MAX = std::numeric_limits<cnt_t>::max();

	const Instance& inst;

	std::vector<Interval<idx_t>> op_junct = {};
	std::vector<Interval<idx_t>> op_level = {};

	std::vector<Junction> juncts = {};
	std::vector<Train> trains = {};
	std::vector<Level> levels = {};

	Flag is_res_req;
	Flag is_res_opt;
	Flag is_res_split;
	
	Preprocess(const Instance& inst, const bool verify=false);
	~Preprocess();
	
	METHOD_N(juncts)
	METHOD_N(levels)
	METHOD_N(trains)

	METHOD_RANGE(juncts, idx_t)
	METHOD_RANGE(levels, idx_t)
	METHOD_RANGE(trains, idx_t)
	
private:
	std::vector<Idx_op> junct_succ = {};
	std::vector<Idx_op> junct_pred = {};

	std::vector<idx_t> level_juncts = {};
	std::vector<Idx_op> level_succ = {};
	std::vector<Idx_op> level_pred = {};

	std::vector<cnt_t> op_count = {};
	std::vector<cnt_t*> res_count = {};
	std::vector<cnt_t> res_count_data = {};

	std::vector<idx_t> level_res = {};
	

	void make_junctions();
	void make_levels();

	void verify_juncts() const;
	void verify_levels() const;

	void make_junctions_bounds();

	void make_count();
	void make_level_res();
	void make_train_res();
	void make_global_res();

	void make_directions();
};

struct Preprocess::Junction
{
	idx_t idx = IDX_MAX;
	idx_t level = IDX_MAX;
	idx_t train = IDX_MAX;

	tim_t time_lb = 0;
	tim_t time_ub = TIM_MAX;

	Array<Idx_op> succ;
	Array<Idx_op> pred;

	METHOD_N(succ);
	METHOD_N(pred);
};


struct Preprocess::Level
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;

	tim_t time_lb = 0;
	tim_t time_ub = TIM_MAX;

	Array<idx_t> juncts;
	Array<Idx_op> succ;
	Array<Idx_op> pred;

	Array<idx_t> res;
	Array<idx_t> res_req;
	Array<idx_t> res_opt;

	METHOD_N(juncts);
	METHOD_N(succ);
	METHOD_N(pred);
};


struct Preprocess::Train
{
	idx_t idx = IDX_MAX;
	idx_t junct_first = IDX_MAX;
	idx_t level_first = IDX_MAX;

	Array<Junction> juncts; 
	Array<Level> levels;

	Flag is_res_req;
	Flag is_res_opt;
	
	METHOD_AFTER(junct)
	METHOD_LAST(junct)
	METHOD_AFTER(level)
	METHOD_LAST(level)

	auto level_range() const { return Range<idx_t>(level_first, level_after()); }
};


struct Preprocess::Idx_op
{
	idx_t idx = IDX_MAX;
	idx_t op = IDX_MAX;

	bool operator==(const Idx_op& other) const 
	{ return (this->idx == other.idx) && (this->op == other.op); }
};