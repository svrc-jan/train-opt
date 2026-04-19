#pragma once

#include <queue>

#include "utils/flag.hpp"
#include "utils/interval.hpp"
#include "instance.hpp"


class Preprocess
{
public:
	struct Op;
	struct Route;
	struct Junction;
	struct Level;
	struct Obj;
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

	std::vector<Op> ops = {};
	std::vector<Route> routes = {};
	std::vector<Junction> juncts = {};
	std::vector<Level> levels = {};
	std::vector<Obj> objs = {};
	std::vector<Train> trains = {};
	

	Flag is_res_req;
	Flag is_res_opt;
	Flag is_res_split;
	Flag is_res_reentry;
	
	Preprocess(const Instance& inst, const bool verify=false);
	~Preprocess();
	
	METHOD_N(ops)
	METHOD_N(routes)
	METHOD_N(juncts)
	METHOD_N(levels)
	METHOD_N(trains)

	METHOD_RANGE(ops, idx_t)
	METHOD_RANGE(routes, idx_t)
	METHOD_RANGE(juncts, idx_t)
	METHOD_RANGE(levels, idx_t)
	METHOD_RANGE(trains, idx_t)
	
private:
	std::vector<Idx_op> junct_succ = {};
	std::vector<Idx_op> junct_pred = {};

	std::vector<idx_t> level_juncts = {};
	std::vector<Idx_op> level_succ = {};
	std::vector<Idx_op> level_pred = {};

	std::vector<idx_t> ops_req = {};
	std::vector<idx_t> route_ops = {};

	Flag is_op_req;
	Flag is_level_req;

	std::vector<cnt_t> op_count = {};
	std::vector<cnt_t*> res_count = {};
	std::vector<cnt_t> res_count_data = {};

	std::vector<idx_t> level_res = {};

	std::queue<idx_t> que;

	void make_junctions();
	void make_levels();

	void verify_juncts() const;
	void verify_levels() const;

	void make_req_levels();
	void make_req_ops();
	void make_routes();
	void make_junct_route();

	void make_junction_bounds();
	void make_level_bounds();
	void make_resource_chunks();

	void make_count();
	void make_level_res();
	void make_reentry_res();
	void make_train_res();
	void make_global_res();

	void make_directions();

	
};


struct Preprocess::Op
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t route = IDX_MAX;
	Interval<idx_t> junct = {IDX_MAX, IDX_MAX};
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
};


struct Preprocess::Route
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	Array<idx_t> ops;
};


struct Preprocess::Junction
{
	idx_t idx = IDX_MAX;
	idx_t level = IDX_MAX;
	idx_t train = IDX_MAX;
	uint8_t req_route_cons = 0;

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
	uint8_t is_req = 1;

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

	idx_t op_first = IDX_MAX;
	idx_t route_first = IDX_MAX;
	idx_t junct_first = IDX_MAX;
	idx_t level_first = IDX_MAX;
	
	Array<Op> ops;
	Array<idx_t> ops_req;
	Array<Route> routes;
	Array<Junction> juncts; 
	Array<Level> levels;
	
	METHOD_N(ops)
	METHOD_N(routes)
	METHOD_N(juncts)
	METHOD_N(levels)

	METHOD_AFTER(op)
	METHOD_AFTER(route)
	METHOD_AFTER(junct)
	METHOD_AFTER(level)

	METHOD_LAST(op)
	METHOD_LAST(route)
	METHOD_LAST(junct)
	METHOD_LAST(level)

	auto levels_range() const { return Range<idx_t>(level_first, level_after()); }
};


struct Preprocess::Idx_op
{
	idx_t idx = IDX_MAX;
	idx_t op = IDX_MAX;

	bool operator==(const Idx_op& other) const 
	{ return (this->idx == other.idx) && (this->op == other.op); }
};