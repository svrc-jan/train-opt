#pragma once

#include <set>
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
	struct Section;
	struct Chunk;
	struct Obj;
	struct Train;

	struct Junct_edge;
	struct Level_edge;
	struct Chunk_state;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef uint16_t cnt_t;

	static const idx_t IDX_MAX = Instance::IDX_MAX;
	static const dur_t DUR_MAX = Instance::DUR_MAX;
	static const tim_t TIM_MAX = Instance::TIM_MAX;
	static const cnt_t CNT_MAX = std::numeric_limits<cnt_t>::max();

	const Instance& inst;

	std::vector<Op> 	  ops 	 = {};
	std::vector<Junction> juncts = {};
	std::vector<Level> 	  levels = {};
	std::vector<Route> 	  routes = {};
	std::vector<Section>  sects  = {};
	std::vector<Chunk>	  chunks = {};
	std::vector<Obj> 	  objs 	 = {};
	std::vector<Train> 	  trains = {};

	std::vector<idx_t> res_n_chunks = {};
	std::vector<idx_t> ops_req = {};

	size_t chunk_direct_merges = 0;
	size_t chunk_parallel_merges = 0;
	
	Preprocess(const Instance& inst, const bool verify=false);
	~Preprocess();
	
	METHOD_N(ops)
	METHOD_N(juncts)
	METHOD_N(levels)
	METHOD_N(routes)
	METHOD_N(sects)
	METHOD_N(chunks)
	METHOD_N(objs)
	METHOD_N(trains)
	

	METHOD_RANGE(ops, idx_t)
	METHOD_RANGE(routes, idx_t)
	METHOD_RANGE(juncts, idx_t)
	METHOD_RANGE(levels, idx_t)
	METHOD_RANGE(chunks, idx_t)
	METHOD_RANGE(objs, idx_t)
	METHOD_RANGE(trains, idx_t)
	
private:
	struct Op_chunk_chain;

	std::vector<Junct_edge> junct_succ = {};
	std::vector<Junct_edge> junct_pred = {};

	std::vector<idx_t>  	level_juncts = {};
	std::vector<Level_edge> level_succ = {};
	std::vector<Level_edge> level_pred = {};

	std::vector<idx_t> route_ops = {};
	std::vector<idx_t> sect_routes = {};

	Flag is_op_req;

	std::vector<Instance::Res> chunk_ops = {};
	std::vector<idx_t> op_chunks = {};

	std::set<idx_t> set_;
	std::queue<idx_t> queue_;

	void make_junctions();
	void make_levels();

	void verify_juncts() const;
	void verify_levels() const;

	void make_req_levels();
	void make_req_ops();

	void make_routes();
	void make_route_junct_level();
	void make_sections();

	void make_resource_chunks();
	void assign_op_chunks();

	void make_objs();

	void make_junction_bounds();
	void make_level_bounds();

	inline size_t n_opt_levels() const;

	bool merge_res_op(std::vector<Op_chunk_chain>& ro, idx_t i, idx_t r, bool match_time, std::set<idx_t>& op_set);
	bool is_op_reachable(const std::set<idx_t>& set_from, idx_t target);
	
	template<bool FWD>
	bool is_op_reachable_temp(const std::set<idx_t>& set_from, idx_t target);
};


struct Preprocess::Op
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t route = IDX_MAX;
	Interval<idx_t> junct = {IDX_MAX, IDX_MAX};
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	Array<idx_t> chunks;
	const Instance::Op* inst = nullptr;
};


struct Preprocess::Junction
{
	idx_t idx = IDX_MAX;

	idx_t level = IDX_MAX;
	idx_t train = IDX_MAX;
	uint8_t is_route = 0;

	tim_t time_lb = 0;
	tim_t time_ub = TIM_MAX;

	Array<Junct_edge> succ;
	Array<Junct_edge> pred;

	METHOD_N(succ);
	METHOD_N(pred);
};


struct Preprocess::Level
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	uint8_t is_req = 0;
	uint8_t is_route = 0;
	uint8_t is_sect = 0;

	tim_t time_lb = 0;
	tim_t time_ub = TIM_MAX;

	Array<idx_t> juncts;
	Array<Level_edge> succ;
	Array<Level_edge> pred;

	METHOD_N(juncts);
	METHOD_N(succ);
	METHOD_N(pred);
};


struct Preprocess::Route
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	Interval<idx_t> junct = {IDX_MAX, IDX_MAX};
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	Array<idx_t> ops;
};


struct Preprocess::Section
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	Array<idx_t> routes;
};


struct Preprocess::Chunk_state
{
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	dur_t rel_time = 0;

	inline bool operator==(const Chunk_state& x) const 
	{ return (level == x.level) && (rel_time == x.rel_time); }
};


struct Preprocess::Chunk
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t res = IDX_MAX;
	Chunk_state state;
	Array<Instance::Res> ops;
};


struct Preprocess::Obj
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t route = IDX_MAX;
	idx_t level = IDX_MAX;
	uint8_t is_bin = 0;
	uint8_t coeff = 0;
	uint8_t n_routes = 1;
	tim_t threshold = 0;

	inline bool operator==(const Obj& x)
	{
		return (level == x.level) && (is_bin == x.is_bin) &&
			(coeff == x.coeff) && (threshold == x.threshold);
	}
};


struct Preprocess::Train
{
	idx_t idx = IDX_MAX;

	idx_t op_first = IDX_MAX;
	idx_t route_first = IDX_MAX;
	idx_t junct_first = IDX_MAX;
	idx_t level_first = IDX_MAX;
	
	Array<Op> 		ops;
	Array<Junction> juncts; 
	Array<Level> 	levels;
	Array<Route> 	routes;
	Array<Section>	sects;
	Array<Obj> 		objs;

	const Instance::Train* inst = nullptr;
	
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


struct Preprocess::Junct_edge
{
	idx_t junct = IDX_MAX;
	idx_t op = IDX_MAX;

	inline bool operator==(const Junct_edge& x) const
	{ return (junct == x.junct) && (op == x.op); }
};

struct Preprocess::Level_edge
{
	idx_t level = IDX_MAX;
	idx_t op = IDX_MAX;
	inline bool operator==(const Level_edge& x) const
	{ return (level == x.level) && (op == x.op); }
};


struct Preprocess::Op_chunk_chain
{
	idx_t op = IDX_MAX;
	dur_t res_time = 0;
	cnt_t prev = CNT_MAX;
	cnt_t next = CNT_MAX;
	cnt_t has_unlock = false;
};


size_t Preprocess::n_opt_levels() const 
{
	size_t count = 0;
	for (auto& level : this->levels) {
		count += (level.is_req ? 0 : 1);
	}
	return count;
}
