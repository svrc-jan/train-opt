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
	
	while (true) {
		idx_t curr_train = this->get_most_conflicting_train();
		if (curr_train == IDX_MAX) {
			cout << "no conflicts";
			break;
		}

		cout << "resolving train: " << curr_train << endl;

		this->expect_cycle = true;
			
		bool conf_added = true;
		while (conf_added) {
			conf_added = this->conf_rslvr->add_conflict(curr_train);
		}
		this->expect_cycle = false;
		
		this->conf_rslvr->freeze_conflicts();
		this->conf_rslvr->clear_constrs();

		cout << "conflicts resolved" << endl;
	}
}



Solver::idx_t Solver::get_most_conflicting_train()
{
	this->chunk_mngr->sync_res();
	
	auto& train_idx = this->chunk_mngr->train_idx;
	auto& time = this->chunk_mngr->time;

	auto trains_range = this->inst.trains_range();
	idx_t conf_count[this->inst.n_trains()];

	for (auto t : trains_range) {
		conf_count[t] = 0;
	}

	for (auto& res : this->chunk_mngr->res) {
		for (size_t i = 0; i < res.size; i++) {
			idx_t a = res.chunks[i];
			auto& a_t = time[a];

			for (size_t j = i + 1; j < res.size; j++) {
				idx_t b = res.chunks[j];
				auto& b_t = time[b];

				if (a_t.end > b_t.start) {
					conf_count[train_idx[a]]++;
					conf_count[train_idx[b]]++;
				}
				else {
					break;
				}
			}
		}
	}

	idx_t max_train = IDX_MAX;
	idx_t max_count = 0;

	for (auto t : trains_range) {
		if (max_count < conf_count[t]) {
			max_count = conf_count[t];
			max_train = t;
		}
	}

	return max_train;
}


void Solver::init_data()
{
	this->init_levels();

	this->chunk_mngr->init_data();
	this->route_plnr->init_data();
	this->conf_rslvr->init_data();
}


void Solver::init_levels()
{
	size_t n_levels = this->prepr.n_levels();
	this->graph_time_dirty.set_n_items(n_levels);
	this->event_graph.set_n_vtx(n_levels);
}


void Solver::sync_graph()
{
	while (true) {
		this->route_plnr->sync_event_graph();
		this->conf_rslvr->sync_graph();

		if (!this->need_graph_sync) {
			return;
		}

		this->graph_time_dirty.clear();
		// cout << "sync graph ";
		auto ret = this->event_graph.sync(this->graph_time_dirty);
		

		if (ret == Event_graph::CYCLE_FOUND) {
			assert(this->expect_cycle);
			this->conf_rslvr->add_cycle_cons();
			// cout << "cycle" << endl;
		}

		else if (ret == Event_graph::TIME_UPDATE) {
			graph_time_dirty.get_true_list(this->need_list);
			for (auto l : this->need_list) {
				this->chunk_mngr->time_change(l);
				this->conf_rslvr->time_change(l);
			}

			// cout << "updates: " << this->need_list.size() << endl;
		}
		else {
			// cout << "no update" << endl;
		}

		this->need_graph_sync = false;
	}
}
