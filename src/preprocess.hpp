#pragma once

#include <set>
#include <map>
#include <queue>

#include "utils/flag.hpp"
#include "utils/interval.hpp"
#include "instance.hpp"


class Preprocess
{
public:
	struct Op;
	struct Junction;
	struct Level;
	struct Route;
	struct Section;
	struct Chunk;
	struct Obj;
	struct Train;

	struct Junct_edge;
	struct Level_edge;
	struct State;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef uint16_t cnt_t;
	typedef std::pair<idx_t, idx_t> idx_pr;

	typedef Instance::Idx_dur Idx_dur;

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

	std::vector<Array<idx_t>> res_chunks = {};
	std::vector<idx_t> ops_req = {};


	size_t chunk_direct_merges = 0;
	size_t chunk_parallel_merges = 0;
	
	Preprocess(const Instance& inst, bool verbose=false, bool verify=false);
	~Preprocess();

	void get_chunk_link_set(std::set<idx_t>& link_set, const Chunk& chunk) const;
	void get_op_links(std::vector<idx_pr>& links, idx_t o) const;
	void get_op_succ_links(std::vector<idx_pr>& links, const idx_pr& o) const;

	
	METHOD_N(ops)
	METHOD_N(juncts)
	METHOD_N(levels)
	METHOD_N(routes)
	METHOD_N(sects)
	METHOD_N(chunks)
	METHOD_N(objs)
	METHOD_N(trains)
	

	METHOD_RANGE(ops, idx_t)
	METHOD_RANGE(juncts, idx_t)
	METHOD_RANGE(levels, idx_t)
	METHOD_RANGE(routes, idx_t)
	METHOD_RANGE(sects, idx_t)
	METHOD_RANGE(chunks, idx_t)
	METHOD_RANGE(objs, idx_t)
	METHOD_RANGE(trains, idx_t)

	inline size_t n_opt_levels() const;
	inline size_t n_inva_junct() const;


	
private:
	enum Chunk_conn { INVALID, DIRECT, PARALLEL };

	std::vector<Junct_edge> junct_succ = {};
	std::vector<Junct_edge> junct_pred = {};
	std::vector<idx_pr> junct_inva_trans = {};

	std::vector<idx_t>  	level_juncts = {};
	std::vector<Level_edge> level_succ = {};
	std::vector<Level_edge> level_pred = {};

	std::vector<idx_t> route_ops = {};
	std::vector<idx_t> sect_routes = {};

	Flag is_op_req;

	std::vector<idx_t> chunk_ops = {};
	std::vector<idx_t> op_chunks = {};

	std::vector<Array<Chunk>> train_chunks = {};
	std::vector<idx_t> res_chunks_data = {};

	std::set<idx_t> set_;
	std::queue<idx_t> queue_;
	std::priority_queue<
		std::pair<tim_t, idx_t>,
		std::vector<std::pair<tim_t, idx_t>>,
		std::greater<std::pair<tim_t, idx_t>>
	> prio_queue;

	void make_junctions();
	void make_levels();

	void verify_juncts() const;
	void verify_levels() const;

	void make_invalid_transitions();
	void make_req_levels();
	void make_req_ops();

	void make_routes();
	void make_route_junct_level();
	void make_sections();
	void make_section_min_dur();

	void make_resource_chunks();
	void assign_op_chunks();

	void verify_chunks();

	void make_objs();

	bool ops_reachable(const std::vector<idx_t>& vec_from, const std::vector<idx_t>& vec_to);
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
	Array<idx_pr> inva_trans;

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
	tim_t min_dur = TIM_MAX;
	uint8_t is_single_route = false;
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	Array<idx_t> routes;

	METHOD_N(routes);
};


struct Preprocess::State
{
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	dur_t dur = 0;

	inline bool operator==(const State& x) const 
	{ return (level == x.level) && (dur == x.dur); }
};


struct Preprocess::Chunk
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t res = IDX_MAX;
	State state;
	Array<idx_t> ops;

	bool operator<(idx_t x) const { return idx < x; }
	bool operator==(idx_t x) const { return idx == x; }
};



struct Preprocess::Obj
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t level = IDX_MAX;
	idx_t route = IDX_MAX;
	uint8_t is_bin = 0;
	uint8_t coeff = 0;
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
	Array<Chunk>*	chunks = nullptr;
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


size_t Preprocess::n_opt_levels() const 
{
	size_t count = 0;
	for (auto& level : this->levels) {
		count += (level.is_req ? 0 : 1);
	}
	return count;
}


size_t Preprocess::n_inva_junct() const
{
	size_t count = 0;
	for (auto& junct : this->juncts) {
		count += (junct.inva_trans.size() > 0);
	}
	return count;
}
