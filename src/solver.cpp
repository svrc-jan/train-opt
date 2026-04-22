#include "solver.hpp"

#include <cmath>
#include "utils/stl_print.hpp"

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env)
{

	this->chunk_mngr = unique_ptr<Chunk_manager>(new Chunk_manager(*this));
	this->route_plnr = unique_ptr<Route_planner>(new Route_planner(*this));
	this->conf_rslvr = unique_ptr<Conflict_resolver>(new Conflict_resolver(*this));
	

	this->init_data();
}


Solver::~Solver()
{

}


void Solver::solve()
{
	this->route_plnr->make_init_routes();
}


void Solver::init_data()
{
	this->init_levels();

	this->route_plnr->init_data();
	this->conf_rslvr->init_data();
}


void Solver::init_levels()
{
	size_t n_levels = this->prepr.n_levels();
	this->graph_time_dirty.set_n_items(n_levels);
}



void Solver::sync_graph_time()
{
	this->route_plnr->sync_op_graph();
	if (!this->need_graph_time_sync) {
		return;
	}

	this->graph_time_dirty.clear();
	if (this->event_graph.sync_time(this->graph_time_dirty)) {
		graph_time_dirty.get_true_list(this->need_list);
		for (auto l : this->need_list) {
			this->chunk_mngr->time_change(l);
		}
	};	

	this->need_graph_time_sync = false;
}
