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

}


void Solver::get_op_changes()
{
	this->op_dirty.clear();

	for (auto o : this->inst.ops_range()) {
		auto& op = this->route_plnr->ops[o];
		if (op.value.changed()) {
			this->op_changes.push_back({op.prepr->idx, op.value.get_change()});
			op.value.snap();

			if (op.value.curr) {
				this->op_active += o;
			}
			else {
				this->op_active -= o;
			}
			this->op_dirty += o;
		}
		

		if (op.succ.changed()) {
			if (op.succ.old != IDX_MAX) {
				this->op_succ_changes -= {o, op.succ.old};
			}
			if (op.succ.curr != IDX_MAX) {
				this->op_succ_changes += {o, op.succ.curr};
			}
			op.succ.snap();
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
	this->op_active.set_n_items(this->inst.n_ops());
}


void Solver::init_levels()
{
	size_t n_levels = this->prepr.n_levels();
	this->event_graph.set_n_vtx(n_levels);
}
