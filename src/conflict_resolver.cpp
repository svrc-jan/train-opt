#include "conflict_resolver.hpp"


#include <format>

#include "utils/aux.hpp"
#include "utils/stl_print.hpp"

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
	tim_t min_time = TIM_MAX;
	idx_pr chunk = {IDX_MAX, IDX_MAX};

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

	assert(chunk.first < this->prepr.n_chunks());
	assert(chunk.second < this->prepr.n_chunks());
	
	auto& is_chunk_active = this->slvr.chunk_mngr->is_active;
	assert(is_chunk_active[chunk.first] && is_chunk_active[chunk.second]);


	bool default_val = true;
	if (chunk.first > chunk.second) {
		swap(chunk.first, chunk.second);
		default_val = false;
	}

	size_t idx = this->confs.size();
	assert(this->confs.size() < IDX_MAX);

	idx_t t1 = train_idx[chunk.first];
	idx_t t2 = train_idx[chunk.second];

	assert(t1 != t2);
	assert(train == IDX_MAX || t1 == train || t2 == train);

	this->slvr.link_graph.get_chain<Link_graph::EITHER>(this->conf_chain, chunk);

	Conflict conf;
	conf.idx = (idx_t)idx;
	conf.active = {true, false}; // curr = true, old = false
	conf.value = {default_val};
	conf.var = this->model.addVar(0, 1, 0, GRB_BINARY, format("conf_{}", idx));

	this->confs.push_back(conf);

	cout << "conflict idx: " << conf.idx << 
		", trains: (" << t1 << ", " << t2 <<
		"), time: " << min_time << 
		", chunks:" << conf.chunks << endl;

	return true;
}


void Conflict_resolver::add_cycle_cons()
{
	auto& event_graph = this->slvr.event_graph;
	assert(event_graph.cycle_found_vtx.size() > 0);

	auto& curr_confs = this->conf_hlpr;
	auto& min_confs = this->conf_hlpr2;

	idx_t min_size = IDX_MAX;

	for (auto v : event_graph.cycle_found_vtx) {
		event_graph.get_cycle_path(this->path, v);

		curr_confs.clear();
		for (auto& x : this->path) {
			if (x.e < EDG_MAX) {
				curr_confs.push_back({x.e, 0});
			}
		}

		if (min_size > curr_confs.size()) {
			min_size = curr_confs.size();
			min_confs = curr_confs;
		}
	}

	assert(min_size >= 2 && min_confs.size() == min_size);

	Cycle_constr cycle_cons;
	cycle_cons.confs = min_confs;

	GRBLinExpr expr = 0;
	for (auto& x : cycle_cons.confs) {
		auto& conf = this->confs[x.first];

		x.second = conf.value.curr;
		expr += conf.to_expr();
	}

	cycle_cons.model = this->model.addConstr(expr <= min_size - 1);
	cycle_cons.model.set(GRB_IntAttr_Lazy, 1);

	this->cycle_constrs.push_back(cycle_cons);
}


bool Conflict_resolver::add_path_cons()
{
	this->sync_values();
	// this->slvr.sync_graph();

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

	path_cons.confs.clear();
	this->slvr.event_graph.get_critical_path(this->path, obj.prepr->level);
	for (auto& x : this->path) {
		if (x.e < EDG_MAX) {
			path_cons.confs.push_back({x.e, 0});
		}
	}
	make_unique(path_cons.confs);

	// cout << "path cons - idx: " << path_cons.obj_idx;
	// if (is_bin) {
	// 	cout << ", binary";
	// }
	// else {
	// 	cout << ", delay: " << path_cons.delay;
	// }
	// cout << ", conf:";

	GRBLinExpr expr = 0;

	for (auto& x : path_cons.confs) {
		auto& conf = this->confs[x.first];
		x.second = conf.value.curr;

		expr += conf.to_expr();
	}
	

	if (is_bin) {
		path_cons.model = this->model.addConstr(expr - path_cons.confs.size() + 1 <= obj.var);
	}
	else {
		path_cons.model = this->model.addConstr((expr - path_cons.confs.size() + 1)*path_cons.delay <= obj.var);
	}
	path_cons.model.set(GRB_IntAttr_Lazy, 3);

	this->path_constrs.push_back(path_cons);
	return true;
}


void Conflict_resolver::sync_graph()
{
	for (auto& conf : this->confs) {
		if (conf.active.changed() || conf.value.changed()) {
			if (conf.active.old) {
				this->make_conf_edges(conf, conf.value.old);
				this->slvr.event_graph.remove_edges(this->conf_edges);
			}

			if (conf.active.curr) {
				this->make_conf_edges(conf, conf.value.curr);
				this->slvr.event_graph.add_edges(this->conf_edges);
			}

			conf.active.snap();
			conf.value.snap();
		}
	}
}


void Conflict_resolver::make_conf_edges(const Conflict& conf, int8_t value)
{
	this->conf_edges.clear();
	for (auto x : conf.chunks) {
		if (value) {
			swap(x.first, x.second);
		}

		this->conf_edges.push_back(this->slvr.chunk_mngr->get_edge(x, conf.idx));
	}
}


void Conflict_resolver::optimize_model()
{
	int status = GBR_EXCEPTION;

	while(true) {
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

		if (status == GRB_INFEASIBLE) {
			this->unfreeze_iis();
		}
		else {
			break;
		}
	}

	this->sync_values();
}


void Conflict_resolver::sync_values()
{
	for (auto& conf : this->confs) {
		if (!conf.active.curr) { continue; }

		conf.value.curr = conf.var.get(GRB_DoubleAttr_X) > 0.5;
	}

	for (auto& obj : this->objs) {
		if (!obj.active) { continue; }

		obj.value = round(obj.var.get(GRB_DoubleAttr_X));
	}
}


void Conflict_resolver::unfreeze_iis()
{
	this->model.computeIIS();

	bool found = false;
	for (auto& conf : this->confs | views::reverse) {
		if (!conf.frozen) { continue; }

		if (conf.value.curr) {
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


bool Conflict_resolver::Constr::is_conf_overlap(const set<idx_t>& x)
{
	for (auto k : this->confs) {
		if (x.contains(k.first)) {
			return true;
		}
	}

	return false;
}

