#pragma once


#include "solver.hpp"


class Solver;

class Conflict_resolver
{
public:
	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;

	typedef Event_graph::edg_t edg_t; 
	typedef Event_graph::vtx_t vtx_t; 
	typedef Event_graph::Edge Edge;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	static constexpr edg_t EDG_MAX = Event_graph::EDG_MAX;
	static constexpr vtx_t VTX_MAX = Event_graph::VTX_MAX;

	const Instance& inst;
	const Preprocess& prepr;
	
	Conflict_resolver(Solver& solver);
	~Conflict_resolver();

	friend class Solver;

private:
	Solver& slvr;
	GRBModel model;

	uint8_t need_conf_graph_sync = false;
	std::set<idx_t> conf_graph_dirty;

	void init_data();
};
