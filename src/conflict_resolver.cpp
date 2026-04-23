#include "conflict_resolver.hpp"


#include <format>


using namespace std;


Conflict_resolver::Conflict_resolver(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver), model(solver.grb_env)
{
	model.set(GRB_IntParam_LazyConstraints, 1);
};


Conflict_resolver::~Conflict_resolver()
{

}


bool Conflict_resolver::add_conflict(idx_t train)
{
	this->slvr.chunk_mngr->sync_res();
	this->sync_obj();

	tim_t min_time = TIM_MAX;
	pair<idx_t, idx_t> chunk = {IDX_MAX, IDX_MAX};

	auto& train_idx = this->slvr.chunk_mngr->train_idx;
	auto& time = this->slvr.chunk_mngr->time;

	for (auto r : this->inst.res_range()) {
		auto& res = this->slvr.chunk_mngr->res[r];

		for (size_t i = 0; i + 1 < res.size; i++) {
			idx_t a = res.chunks[i];
			idx_t b = res.chunks[i + 1];

			if ((train < IDX_MAX) && (train_idx[a] != train) && (train_idx[b] != train)) {
				continue;
			}

			auto& a_t = time[a];
			auto& b_t = time[b];

			if (b_t.start >= min_time) {
				break;
			}

			if (a_t.end > b_t.start) {
				chunk = {res.chunks[i], res.chunks[i + 1]};
				min_time = b_t.start;
				break;
			}
		}
	}

	if (min_time == TIM_MAX) {
		return false;
	}

	assert(chunk.first < this->prepr.n_chunks() && chunk.first < this->prepr.n_chunks());
	assert(train_idx[chunk.first] != train_idx[chunk.second]);

	auto& is_chunk_active = this->slvr.chunk_mngr->is_active;
	assert(is_chunk_active[chunk.first] && is_chunk_active[chunk.second]);


	bool default_val = true;
	if (chunk.first > chunk.second) {
		swap(chunk.first, chunk.second);
		default_val = false;
	}

	size_t idx = this->confs.size();
	assert(idx < IDX_MAX);
	
	auto insert_ret = this->chunk_pairs.insert(chunk);
	assert(insert_ret.second);

	auto var = this->model.addVar(0, 1, 0, GRB_BINARY, format("conf_{}", idx));

	Conflict conf = {
		.idx = (idx_t)idx,
		.active = true,
		.value = default_val,
		.frozen = false,
		.chunk = chunk,
		.ordering = {
			this->slvr.chunk_mngr->get_ordering(chunk.first, chunk.second),
			this->slvr.chunk_mngr->get_ordering(chunk.second, chunk.first)
		},
		.var = var,
		.links = {}
	};

	this->confs.push_back(conf);

	this->conf_graph_dirty.insert(idx);
	this->need_graph_sync = true;

	this->conf_link_dirty.insert(idx);
	this->need_link_sync = true;

	cout << "conflict idx: " << conf.idx <<
		", chunks: " << chunk.first << ":" << chunk.second <<
		" (time: " << min_time << ")" << endl;

	return true;
}


void Conflict_resolver::freeze_conflicts()
{
	for (auto& conf : this->confs) {
		conf.freeze();
	}
}

void Conflict_resolver::clear_constrs()
{
	for (auto& constr : this->cycle_constrs) {
		this->model.remove(constr.model);
	}

	this->cycle_constrs.clear();

	for (auto& constr : this->path_constrs) {
		this->model.remove(constr.model);
	}

	this->path_constrs.clear();
}

void Conflict_resolver::sync_values()
{
	this->sync_state();
	this->sync_model();
	
	if (!this->need_value_sync) {
		return;
	}

	for (auto& conf : this->confs) {
		if (!conf.active) { continue; }

		bool new_value = conf.var.get(GRB_DoubleAttr_X) > 0.5;
		if (conf.value != new_value) {
			this->conf_graph_dirty.insert(conf.idx);
			this->need_graph_sync = true;

			conf.value = new_value;
		}
	}

	for (auto& obj : this->objs) {
		if (!obj.active) { continue; }

		tim_t new_value = round(obj.var.get(GRB_DoubleAttr_X));
		if (obj.value != new_value) {
			obj.value = new_value;
			this->need_obj_sync = true;
		}
	}

	this->need_value_sync = false;
}


void Conflict_resolver::sync_obj()
{
	if (!this->need_obj_sync) {
		return;
	}
	
	bool cons_added = true;
	while (cons_added)	{
		cons_added = this->add_path_cons();
	}
	
	this->need_obj_sync = false;
}


void Conflict_resolver::sync_state()
{
	this->slvr.chunk_mngr->sync_state();
	if (!this->need_state_sync) { return; }

	auto& is_chunk_active = this->slvr.chunk_mngr->is_active;

	for (auto k : this->conf_state_dirty) {
		auto& conf = this->confs[k];
		bool new_active = is_chunk_active[conf.chunk.first] &&
			is_chunk_active[conf.chunk.second];

		bool changed = (conf.active == new_active);
		if (new_active) {
			pair<Ordering, Ordering> new_ordering;
			new_ordering.first = this->slvr.chunk_mngr->get_ordering(conf.chunk.first, conf.chunk.second);
			new_ordering.second = this->slvr.chunk_mngr->get_ordering(conf.chunk.second, conf.chunk.first);

			if (conf.ordering.first != new_ordering.first) {
				conf.ordering.first = new_ordering.first;
				changed = true;
			}

			if (conf.ordering.second != new_ordering.second) {
				conf.ordering.second = new_ordering.second;
				changed = true;
			}
		}

		if (changed) {
			this->conf_graph_dirty.insert(k);
			this->need_graph_sync = true;

			this->conf_purge.insert(k);
			this->need_conf_purge = true;
		}
	}

	this->conf_state_dirty.clear();
	this->need_state_sync = false;
}


void Conflict_resolver::sync_graph()
{
	this->sync_state();
	this->sync_values();

	if (!this->need_graph_sync) { return; }
	
	for (auto k : this->conf_graph_dirty) {
		auto& conf = this->confs[k];

		Edge new_edge = conf.to_edge();
		if (conf.edge != new_edge) {
			bool ret = this->slvr.event_graph.update_edge(conf.edge, new_edge);
			assert(ret);

			this->slvr.graph_change();

			conf.edge = new_edge;
		}
	}

	this->conf_graph_dirty.clear();
	this->need_graph_sync = false;
}


void Conflict_resolver::purge_conf()
{
	this->sync_state();
	if (!this->need_conf_purge) {
		return;
	}

	if (this->conf_purge.empty()) {
		this->need_conf_purge = false;
		return;
	}

	size_t i = 0;
	while (i < this->cycle_constrs.size()) {
		auto& constr = this->cycle_constrs[i];
		bool purge = constr.is_conf_overlap(this->conf_purge);

		if (purge) {
			this->model.remove(constr.model);
			constr = this->cycle_constrs.back();
			this->cycle_constrs.pop_back();
		}
		else {
			i++;
		}
	}

	i = 0;
	while (i < this->path_constrs.size()) {
		auto& constr = this->path_constrs[i];
		bool purge = constr.is_conf_overlap(this->conf_purge);

		if (purge) {
			this->model.remove(constr.model);
			constr = this->path_constrs.back();
			this->path_constrs.pop_back();
		}
		else {
			i++;
		}
	}

	this->conf_purge.clear();
	this->need_conf_purge = false;
}


void Conflict_resolver::add_cycle_cons()
{
	auto& event_graph = this->slvr.event_graph;
	assert(event_graph.cycle_found_vtx.size() > 0);

	idx_t min_size = IDX_MAX;

	set<idx_t> curr_set;
	set<idx_t> min_set;
		
	for (auto v : event_graph.cycle_found_vtx) {
		event_graph.get_cycle_path(this->path, v);

		size_t curr_size = 0;
		curr_set.clear();
		for (auto& x : this->path) {
			if (x.e < EDG_MAX) {
				curr_set.insert(x.e);
				curr_size++;
				
				if (curr_size >= min_size) {
					break;
				}
			}
		}

		if (min_size > curr_size) {
			min_size = curr_size;
			min_set = curr_set;
		}
	}

	assert(min_size >= 2 && min_set.size() == min_size);

	Cycle_constr cycle_cons;
	cycle_cons.conf_set = min_set;
	

	// cout << "cycle cons - conf:";
	GRBLinExpr expr = 0;

	cycle_cons.conf_values.clear();
	for (auto k : min_set) {
		auto& conf = this->confs[k];

		cycle_cons.conf_values.push_back(conf.value);
		expr += conf.to_expr();

		// cout << " " << k << ":" << (int)conf.value;
	}

	// cout << endl;

	cycle_cons.model = this->model.addConstr(expr <= min_size - 1);
	cycle_cons.model.set(GRB_IntAttr_Lazy, 1);

	this->cycle_constrs.push_back(cycle_cons);

	this->need_model_sync = true;
}


bool Conflict_resolver::add_path_cons()
{
	this->sync_values();
	this->slvr.sync_graph();

	tim_t max_diff = 0;
	tim_t max_delay = 0;
	idx_t max_idx = IDX_MAX;

	for (auto& obj : this->objs) {
		if (!obj.active) { continue; }
		if (obj.prepr->is_bin && (obj.value == 1)) { continue; }

		tim_t time = this->slvr.time(obj.prepr->level);
		if (time <= obj.prepr->threshold) {	continue; }

		tim_t delay = time - obj.prepr->threshold;
		if (obj.prepr->is_bin) {
			if (max_diff < obj.prepr->coeff) {
				max_diff = obj.prepr->coeff;
				max_idx = obj.idx;
			}
		}
		else {
			if (delay <= obj.value) { continue;	}
			
			tim_t diff = obj.prepr->coeff*(delay - obj.value);
			if (max_diff < diff) {
				max_diff = diff;
				max_delay = delay;
				max_idx = obj.idx;
			}
		}
	}

	if (max_idx == IDX_MAX) {
		return false;
	}

	auto& obj = this->objs[max_idx];
	bool is_bin = obj.prepr->is_bin;

	Path_constr path_cons;

	path_cons.obj_idx = max_idx;
	path_cons.is_bin = is_bin;
	path_cons.delay = (is_bin ? 0 : max_delay);

	path_cons.conf_set.clear();
	this->slvr.event_graph.get_critical_path(this->path, obj.prepr->level);
	for (auto& x : this->path) {
		if (x.e < EDG_MAX) {
			path_cons.conf_set.insert(x.e);
		}
	}

	// cout << "path cons - idx: " << path_cons.obj_idx;
	// if (is_bin) {
	// 	cout << ", binary";
	// }
	// else {
	// 	cout << ", delay: " << path_cons.delay;
	// }
	// cout << ", conf:";

	GRBLinExpr expr = 0;

	size_t count = 0;
	path_cons.conf_values.clear();
	for (auto k : path_cons.conf_set) {
		auto& conf = this->confs[k];

		path_cons.conf_values.push_back(conf.value);
		expr += conf.to_expr();

		// cout << " " << k << ":" << (int)conf.value;
		count += 1;
	}
	
	// cout << endl;

	if (is_bin) {
		path_cons.model = this->model.addConstr(expr - count + 1 <= obj.var);
	}
	else {
		path_cons.model = this->model.addConstr((expr - count + 1)*path_cons.delay <= obj.var);
	}
	path_cons.model.set(GRB_IntAttr_Lazy, 2);

	this->path_constrs.push_back(path_cons);
	this->need_model_sync = true;

	return true;
}


void Conflict_resolver::sync_links()
{
	if (!this->need_link_sync) {
		return;
	}

	auto& train_idx = this->slvr.chunk_mngr->train_idx;

	set<idx_t> c_lk_first;
	set<idx_t> c_lk_second;
	set<idx_t> link_confs;

	for (auto k : this->conf_link_dirty) {
		auto conf = this->confs[k];

		for (auto& link : conf.links) {
			this->model.remove(link.cons);

			if (link.idx.first == k) {
				this->confs[link.idx.second].links.erase(link);
			}
			else {
				assert(link.idx.second == k);
				this->confs[link.idx.first].links.erase(link);
			}
		}

		conf.links.clear();

		idx_t c1 = conf.chunk.first;
		idx_t c2 = conf.chunk.second;

		idx_t t1 = train_idx[t1];
		idx_t t2 = train_idx[t2];

		c_lk_first = this->slvr.chunk_mngr->link[c1].get_combination();
		c_lk_second = this->slvr.chunk_mngr->link[c2].get_combination();

		link_confs.clear();

		for (auto c1_lk : c_lk_first) {
			for (auto k_lk : this->chunk_conf[c1_lk]) {
				auto& conf_lk = this->confs[k_lk];
				assert(conf_lk.chunk.first == c1_lk || conf_lk.chunk.second == c1_lk);

				if (train_idx[conf_lk.chunk.first] == t2) {
					assert(train_idx[conf_lk.chunk.second] == t1);

					if (c_lk_second.contains(conf_lk.chunk.first)) {
						link_confs.insert(k_lk);
					}
				}

				if (train_idx[conf_lk.chunk.second] == t2) {
					assert(train_idx[conf_lk.chunk.first] == t1);

					if (c_lk_second.contains(conf_lk.chunk.second)) {
						link_confs.insert(k_lk);
					}
				}
			}
		}

		for (auto k_lk : link_confs) {
			auto& conf_lk = this->confs[k_lk];
			
			Link link(k, k_lk);
			link.cons = this->model.addConstr(conf.var == conf_lk.var,
				format("link_{}_{}", k, k_lk));

			conf.links.insert(link);
			conf_lk.links.insert(link);
		}
	}

	this->conf_link_dirty.clear();
	this->need_link_sync = false;
}


void Conflict_resolver::sync_model()
{	
	this->sync_links();
	if (!this->need_model_sync) {
		return;
	}

	while (true) {
		bool feasible = this->optimize_model();
		if (feasible) {
			break;
		}
		this->unfreeze_iis();
	}
	

	this->need_value_sync = true;
	this->need_model_sync = false;
}

bool Conflict_resolver::optimize_model()
{
	int status = GBR_EXCEPTION;
	try {
		this->model.update();
		this->model.optimize();
		status =  this->model.get(GRB_IntAttr_Status);
	}
	catch (GRBException ex) {
		cout << "Conflict_resolver: gurobi exception " << ex.getMessage() << 
			", code = " << ex.getErrorCode() << endl;
		exit(1);
	}
	
	if ((status != GRB_OPTIMAL) && (status != GRB_INFEASIBLE)) {
		cout << "Conflict_resolver: unexpected optimization status = " << status << endl;
		exit(1);
	}

	bool feasible = (status == GRB_OPTIMAL);
	return feasible;
}


void Conflict_resolver::unfreeze_iis()
{
	this->model.computeIIS();

	bool found = false;
	for (auto& conf : this->confs | views::reverse) {
		if (!conf.frozen) { continue; }

		if (conf.value) {
			if (conf.var.get(GRB_IntAttr_IISLB)) {
				found = true;
			}
		}
		else {
			if (conf.var.get(GRB_IntAttr_IISUB)) {
				found = true;
			}
		}

		if (found) {
			cout << "unfreeze IIS: " << conf.idx << endl;
			conf.unfreeze();
			break;
		}
	}

	assert(found);
}


void Conflict_resolver::init_data()
{
	this->init_chunks();
	this->init_objs();
}


void Conflict_resolver::init_chunks()
{
	this->chunk_conf.resize(this->prepr.n_chunks());
}


void Conflict_resolver::init_objs()
{
	this->objs.resize(this->prepr.n_objs());

	for (auto i : this->prepr.objs_range()) {
		auto& obj = this->objs[i];
		obj.idx = i;
		obj.active = true;
		obj.prepr = &this->prepr.objs[i];

		string name = format("obj_{}", obj.idx);
		if (obj.prepr->is_bin) {
			obj.var = this->model.addVar(0, 1, obj.prepr->coeff, GRB_BINARY, name);
		}
		else {
			obj.var = this->model.addVar(0, GRB_INFINITY, obj.prepr->coeff, GRB_CONTINUOUS, name);
		}

		idx_t l = obj.prepr->level;
		assert(!this->level_obj.contains(l));

		this->level_obj[l] = i;
	}
}


void Conflict_resolver::Conflict::freeze()
{
	if (this->frozen) { return; }

	if (this->value) {
		this->var.set(GRB_DoubleAttr_LB, 1.0);
	}
	else {
		this->var.set(GRB_DoubleAttr_UB, 0.0);
	}

	this->frozen = true;
}


void Conflict_resolver::Conflict::unfreeze()
{
	if (!this->frozen) { return; }

	this->var.set(GRB_DoubleAttr_LB, 0.0);
	this->var.set(GRB_DoubleAttr_UB, 1.0);

	this->frozen = false;
}

Event_graph::Edge Conflict_resolver::Conflict::to_edge() const
{
	if (!this->active) {
		return Edge();
	}

	Edge edge;
	if (this->value) {
		edge = this->ordering.first.to_edge(this->idx);
	}
	else {
		edge = this->ordering.second.to_edge(this->idx);
	}

	return edge;
};


bool Conflict_resolver::Constr::is_conf_overlap(const set<idx_t>& x)
{
	if (this->conf_set.size() < x.size()) {
		for (auto k : this->conf_set) {
			if (x.contains(k)) {
				return true;
			}
		}
	}
	else {
		for (auto k : x) {
			if (this->conf_set.contains(k)) {
				return true;
			}
		}
	}

	return false;
}
