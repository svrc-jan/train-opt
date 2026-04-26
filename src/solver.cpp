#include "solver.hpp"

#include <cmath>
#include "utils/stl_print.hpp"

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), link_graph(prepr), grb_env(grb_env)
{

	this->chunk_mngr = unique_ptr<Chunk_manager>(new Chunk_manager(*this));
	this->route_plnr = unique_ptr<Route_planner>(new Route_planner(*this));
	this->conf_rslvr = unique_ptr<Conflict_resolver>(new Conflict_resolver(*this));
	
	this->init_data();
}


Solver::~Solver()
{

}


void Solver::plan_routes()
{
	this->route_plnr->get_random_ops(0.5);

	this->route_plnr->sync_graph();
	bool ret = this->sync_event_graph();
	assert(!ret);

	this->sync_chunk_mngr_state();
	this->sync_link_graph();

	this->route_plnr->snap_ops();
}


void Solver::solve()
{
	for (auto t : this->inst.trains_range()) {
		this->resolve_conflicts(t);
	}
}


void Solver::resolve_conflicts(idx_t t)
{
	while (true) {
		this->conf_rslvr->optimize_model();
		this->conf_rslvr->sync_graph();
		bool cycle_found = this->sync_event_graph();
		if (cycle_found) {
			this->conf_rslvr->add_cycle_cons();
			continue;
		}

		bool path_added = this->conf_rslvr->add_path_cons();
		if (path_added) {
			continue;
		}

		bool conf_added = this->conf_rslvr->add_conflict(t);
		if (!conf_added) {
			break;
		}
	}
	
}


bool Solver::sync_event_graph()
{
	auto ret = this->event_graph.sync(this->time_dirty);
	return ret == Event_graph::CYCLE_FOUND;
}


void Solver::sync_chunk_mngr_state()
{
	this->chunk_mngr->sync_state(
		this->route_plnr->op_dirty,
		this->route_plnr->op_active.curr);
}


void Solver::sync_link_graph()
{
	this->get_op_changes();
	this->link_graph.sync(this->op_changes, this->op_succ_changes);
}


void Solver::get_op_changes()
{
	this->route_plnr->op_dirty.get_true_list(this->list_hlpr);
	for (auto o : this->list_hlpr) {
		if (this->route_plnr->op_active.curr[o]) {
			this->op_changes += o;
		}
		else {
			this->op_changes -= o;
		}
	}
	
	for (auto o : this->inst.ops_range()) {
		auto& succ = this->route_plnr->op_succ[o];
		if (succ.changed()) {
			if (succ.old != IDX_MAX) {
				this->op_succ_changes -= {o, succ.old};
			}
			if (succ.curr != IDX_MAX) {
				this->op_succ_changes += {o, succ.curr};
			}
		}	
	}
}


void Solver::init_data()
{
	this->init_levels();

	this->chunk_mngr->init_data();
	this->route_plnr->init_data();
	this->conf_rslvr->init_data();
}


void Solver::init_ops()
{

}


void Solver::init_levels()
{
	size_t n_levels = this->prepr.n_levels();
	this->event_graph.set_n_vtx(n_levels);
	this->time_dirty.set_n_items(n_levels);
}
