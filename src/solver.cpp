#include "solver.hpp"

#include <cmath>

#include "utils/aux.hpp"
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
	this->route_plnr->get_random_ops(0);
	
	this->route_plnr->sync_graph();
	bool ret = this->sync_event_graph();

	assert(!ret);

	this->sync_link_graph();
	this->chunk_mngr->op_change(this->route_plnr->op_dirty);
	this->chunk_mngr->sync_state();

	this->route_plnr->snap_ops();
}


Solver::Optimize_state Solver::feasible_solve()
{
	bool ret = this->sync_event_graph();
	assert(!ret);

	this->chunk_mngr->sync_time();
	this->chunk_mngr->sync_res();

	while (true) {
		idx_t curr_chunk = this->choose_earliest_conflicting_chunk();
		if (curr_chunk == IDX_MAX) {
			break;
		}

		auto ret = this->resolve_conflicts_chunk(curr_chunk);
		if (ret == CUTOFF) {
			return CUTOFF;
		}

		this->conf_rslvr->freeze_conflicts();
		this->conf_rslvr->clear_constrs(true);
	}

	// cout << "conflicts resolved" << endl;
	cout << "feasible, objective: " << this->conf_rslvr->get_obj_val() << endl;
	
	return OPTIMAL;
}


void Solver::improving_solve()
{
	bool ret = this->sync_event_graph();
	assert(!ret);

	tim_t ub;
	ub = this->conf_rslvr->get_obj_val();
	
	this->conf_rslvr->set_obj_ub(ub);

	auto& crit_mp = this->conf_rslvr->crit_path_count;
	this->conf_rslvr->make_crit_path_count();

	while (true) {
		auto crit_entry = get_max_entry(crit_mp);
		if (crit_entry.second == 0) {
			break;
		}

		crit_mp[crit_entry.first] = 0;
		this->conf_rslvr->unfreeze_train_confs(crit_entry.first);


		auto ret = this->sync_conf_model();
		if (ret == CUTOFF) {
			continue;
		}

		this->conf_rslvr->freeze_conflicts();
		this->conf_rslvr->clear_constrs(true);
		
		ret = this->feasible_solve();
		if (ret == CUTOFF) {
			continue;
		}


		tim_t new_ub = this->conf_rslvr->get_obj_val();
		if (new_ub < ub) {
			ub = new_ub;
			this->conf_rslvr->set_obj_ub(ub);
			this->conf_rslvr->make_crit_path_count();
		}
	}
}




Solver::Optimize_state Solver::resolve_conflicts_chunk(idx_t c)
{
	while (true) {
		auto ret = this->sync_conf_model();
		if (ret == CUTOFF) {
			return CUTOFF;
		}

		this->chunk_mngr->sync_time();
		this->chunk_mngr->sync_res();

		idx_pr conf = this->conf_rslvr->find_conflict_chunk(c);
		if (conf.first == IDX_MAX || conf.second == IDX_MAX) {
			break;
		}
		this->conf_rslvr->add_conflict(conf);
	}

	return OPTIMAL;
}


Solver::Optimize_state Solver::sync_conf_model()
{
	bool path_added = true;
	while (path_added) {
		auto ret = this->conf_rslvr->optimize_model();
		if (ret == CUTOFF) {
			return CUTOFF;
		}

		this->conf_rslvr->sync_graph();

		bool cycle_found = this->sync_event_graph();
		if (cycle_found) {
			this->conf_rslvr->add_cycle_cons();
			continue;
		}

		path_added = this->conf_rslvr->add_path_cons();
	}

	return OPTIMAL;
}


void Solver::resolve_conflicts_train(idx_t t)
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

		this->chunk_mngr->sync_time();
		this->chunk_mngr->sync_res();

		idx_pr conf = this->conf_rslvr->find_conflict_train(t);
		if (conf.first == IDX_MAX || conf.second == IDX_MAX) {
			break;
		}
		this->conf_rslvr->add_conflict(conf);
	}
}


bool Solver::sync_event_graph()
{
	auto ret = this->event_graph.sync(this->time_dirty);
	if (ret == Event_graph::CYCLE_FOUND) {
		return true;
	}

	this->chunk_mngr->time_change(this->time_dirty);

	return false;
}


void Solver::sync_chunk_mngr_state()
{
	this->chunk_mngr->sync_state();
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


Solver::idx_t Solver::choose_most_conflicting_train(double stretch)
{
	tim_t conf_sum[this->inst.n_trains()];
	for (auto t : this->inst.trains_range()) {
		conf_sum[t] = 0;
	}

	for (auto r : this->inst.res_range()) {
		auto& res = this->chunk_mngr->res[r];

		idx_t* res_end = res.chunks + res.size;
		for (idx_t* a = res.chunks; a < res_end; a++) {
			auto& time_a = this->chunk_mngr->time[*a];
			idx_t train_a = this->chunk_mngr->train_idx[*a];

			if (time_a.start == TIM_MAX) {
				break;
			}

			tim_t stretch_a = time_a.start + round((1.0 + stretch)*(time_a.end - time_a.start));

			for (idx_t* b = a + 1; b < res_end; b++) {
				auto& time_b = this->chunk_mngr->time[*b];
				idx_t train_b = this->chunk_mngr->train_idx[*b];
				
				if (time_b.start == TIM_MAX) {
					break;
				}
				
				if (train_a == train_b) {
					continue;
				}
			
				if (time_a.end > time_b.start) {
					tim_t overlap = stretch_a - time_b.start;
					conf_sum[train_a] += overlap;
					conf_sum[train_b] += overlap;
				}
				else {
					break;
				}
			}
		}
	}

	tim_t max_sum = 0;
	idx_t max_train = IDX_MAX;

	for (auto t : this->inst.trains_range()) {
		if (max_sum < conf_sum[t]) {
			max_sum = conf_sum[t];
			max_train = t;
		}
	}


	return max_train;
}


Solver::idx_t Solver::choose_most_conflicting_chunk(double stretch)
{
	tim_t conf_sum[this->prepr.n_chunks()];
	for (auto c : this->prepr.chunks_range()) {
		conf_sum[c] = 0;
	}

	for (auto r : this->inst.res_range()) {
		auto& res = this->chunk_mngr->res[r];

		idx_t* res_end = res.chunks + res.size;
		for (idx_t* a = res.chunks; a < res_end; a++) {
			auto& time_a = this->chunk_mngr->time[*a];
			idx_t train_a = this->chunk_mngr->train_idx[*a];

			if (time_a.start == TIM_MAX) {
				break;
			}

			tim_t stretch_a = time_a.start + round((1.0 + stretch)*(time_a.end - time_a.start));

			for (idx_t* b = a + 1; b < res_end; b++) {
				auto& time_b = this->chunk_mngr->time[*b];
				idx_t train_b = this->chunk_mngr->train_idx[*b];
				
				if (time_b.start == TIM_MAX) {
					break;
				}
				
				if (train_a == train_b) {
					continue;
				}
			
				if (time_a.end > time_b.start) {
					tim_t overlap = stretch_a - time_b.start;
					conf_sum[*a] += overlap;
					conf_sum[*b] += overlap;
				}
				else {
					break;
				}
			}
		}
	}

	tim_t max_sum = 0;
	idx_t max_chunk = IDX_MAX;

	for (auto c : this->prepr.chunks_range()) {
		if (max_sum < conf_sum[c]) {
			max_sum = conf_sum[c];
			max_chunk = c;
		}
	}


	return max_chunk;
}



Solver::idx_t Solver::choose_earliest_conflicting_chunk()
{
	tim_t min_time = TIM_MAX;
	tim_t min_chunk = IDX_MAX;

	auto& train_idx = this->chunk_mngr->train_idx;
	auto& time = this->chunk_mngr->time;

	for (auto r : this->inst.res_range()) {
		auto& res = this->chunk_mngr->res[r];

		for (size_t i = 0; i + 1 < res.size; i++) {
			idx_t a = res.chunks[i];
			idx_t b = res.chunks[i + 1];

			if ((train_idx[a] == train_idx[b])) {
				continue;
			}

			auto& a_t = time[a];
			auto& b_t = time[b];

			if (b_t.start >= min_time) {
				break;
			}

			if (a_t.end > b_t.start) {
				min_time = b_t.start;
				min_chunk = a;
				break;
			}
		}
	}

	if (min_chunk < IDX_MAX) {
		// cout << "choosing chunk: " << min_chunk << ", time: " << min_time << endl;
	}

	return min_chunk;
}