#include "conflict_resolver.hpp"


#include <format>

#include "utils/aux.hpp"
#include "utils/stl_print.hpp"

#ifndef GBR_EXCEPTION
#define GBR_EXCEPTION 20
#endif


using namespace std;


Conflict_resolver::Conflict_resolver(const Preprocess& prepr, Link_graph& link_graph, 
	Chunk_manager& chunk_mrng, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), link_graph(link_graph), 
	  chunk_mngr(chunk_mngr), model(GRBModel(grb_env))
{
	this->event_graph.set_n_vtx(this->prepr.n_levels());
	this->init_model();
};


Conflict_resolver::~Conflict_resolver()
{

}


void Conflict_resolver::add_conflict(idx_pr chunk)
{
	assert(chunk.first < this->prepr.n_chunks());
	assert(chunk.second < this->prepr.n_chunks());

	bool default_val = false;
	if (chunk.first > chunk.second) {
		swap(chunk.first, chunk.second);
		default_val = true;
	}

	size_t idx = this->confs.size();
	assert(this->confs.size() < IDX_MAX);

	auto& train_idx = this->chunk_mngr.train_idx;

	idx_t t1 = train_idx[chunk.first];
	idx_t t2 = train_idx[chunk.second];

	assert(t1 < t2);

	Conflict conf;
	conf.idx = (idx_t)idx;
	conf.frozen = false;
	conf.active = {true, false};
	conf.value = {default_val};
	conf.var = this->model.addVar(0, 1, 0, GRB_BINARY, format("conf_{}", idx));

	this->link_graph.get_chain_conf(chunk, conf.chunks);

	this->confs.push_back(conf);
}


void Conflict_resolver::make_cycle_cons()
{
	assert(this->event_graph.cycle_found_vtx.size() > 0);

	auto& curr_confs = this->conf_hlpr;
	auto& min_confs = this->conf_hlpr2;

	idx_t min_size = IDX_MAX;

	for (auto v : this->event_graph.cycle_found_vtx) {
		this->event_graph.get_cycle_path(this->path, v);

		curr_confs.clear();
		for (auto& x : this->path) {
			if (x.e < EDG_MAX) {
				curr_confs.push_back({x.e, 0});
			}
		}

		make_unique(curr_confs);

		if (min_size > curr_confs.size()) {
			min_size = curr_confs.size();
			min_confs = curr_confs;
		}
	}

	assert(min_size >= 2 && min_confs.size() == min_size);

	Cycle_constr cycle_cons;
	cycle_cons.confs = min_confs;

	for (auto& x : cycle_cons.confs) {
		auto& conf = this->confs[x.idx];
		x.value = conf.value.curr;
	}

	this->cycle_constrs.push_back(cycle_cons);
	this->add_model_cycle_cons(cycle_cons);
}


bool Conflict_resolver::make_path_cons()
{
	this->sync_values();
	// this->slvr.sync_graph();

	tim_t max_diff = 0;
	tim_t max_delay = 0;
	idx_t max_idx = IDX_MAX;

	for (auto& obj : this->objs) {
		if (obj.is_bin && (obj.value == 1)) {
			continue;
		}

		tim_t time = this->event_graph.time[obj.level];
		if (time <= obj.threshold) {
			continue;
		}

		tim_t delay = time - obj.threshold;
		if (obj.is_bin) {
			if (max_diff < obj.coeff) {
				max_diff = obj.coeff;
				max_idx = obj.idx;
			}
		}
		else {
			if (delay <= obj.value) {
				continue;
			}
			
			tim_t diff = obj.coeff*(delay - obj.value);
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

	Path_constr path_cons;

	path_cons.obj_idx = max_idx;
	path_cons.delay = (obj.is_bin ? 0 : max_delay);

	path_cons.confs.clear();
	this->event_graph.get_critical_path(this->path, obj.level);
	for (auto& x : this->path) {
		if (x.e < EDG_MAX) {
			path_cons.confs.push_back({x.e, 0});
		}
	}

	make_unique(path_cons.confs);

	for (auto& x : path_cons.confs) {
		auto& conf = this->confs[x.idx];
		x.value = conf.value.curr;
	}

	this->path_constrs.push_back(path_cons);
	this->add_model_path_cons(path_cons);

	return true;
}


void Conflict_resolver::sync_graph()
{
	for (auto& conf : this->confs) {
		if (conf.active.changed() || conf.value.changed()) {
			if (conf.active.old) {
				this->make_conf_edges(conf, conf.value.old);
				this->event_graph.remove_edges(this->conf_edges);
			}

			if (conf.active.curr) {
				this->make_conf_edges(conf, conf.value.curr);
				this->event_graph.add_edges(this->conf_edges);
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

		auto& state_a = this->chunk_mngr.state[x.first];
		auto& state_b = this->chunk_mngr.state[x.second];

		this->conf_edges.push_back({{state_a.level.end, state_b.level.end}, state_a.dur, conf.idx});
	}
}


void Conflict_resolver::set_ops(const Flag& op_active)
{
	Flag op_change(this->inst.n_ops());
	op_change.fill(true);

	this->link_graph.op_change(op_change);
	this->link_graph.sync_links(op_active);

	this->chunk_mngr.op_change(op_change);
	this->chunk_mngr.sync_state(op_active);

	this->event_graph.clear_edges();

	op_active.get_true_list(this->flag_list);
	for (auto o : this->flag_list) {
		auto& op = this->prepr.ops[o];

		this->event_graph.set_time_lb(op.level.start, op.inst->start_lb);
		this->event_graph.add_edge({op.level, op.inst->dur, EDG_MAX});

		if (op.inst->obj != IDX_MAX) {
			auto& obj_i = this->inst.objs[op.inst->obj];

			Obj obj;

			obj.idx = (idx_t)this->objs.size();
			obj.level = op.level.start;
			obj.is_bin = obj_i.increment > 0;
			obj.coeff = obj.is_bin ? obj_i.increment : obj_i.coeff;
			obj.threshold = obj_i.threshold;

			if (obj.is_bin) {
				obj.var = this->model.addVar(0, 1, obj.coeff, GRB_BINARY, format("obj_{}", obj.idx));
			}
			else {
				obj.var = this->model.addVar(0, GRB_INFINITY, obj.coeff, GRB_CONTINUOUS, 
					format("obj_{}", obj.idx));
			}

			this->objs.push_back(obj);
		}
	}
}


int Conflict_resolver::optimize_model()
{
	int status = GBR_EXCEPTION;
	int state = FAILED;

	bool done = false;
	while(!done) {
		try {
			if (this->need_model_update) {
				this->model.update();
				this->need_model_update = false;
			}

			this->model.optimize();
			status = this->model.get(GRB_IntAttr_Status);
		}
		catch (GRBException ex) {
			cout << "Conflict_resolver: gurobi exception " << ex.getMessage() << 
				", code = " << ex.getErrorCode() << endl;
			exit(1);
		}
		
		switch (status) {
		  case GRB_OPTIMAL:
			this->sync_values();
			done = true;
			state = OPTIMAL;
			break;

		  case GRB_CUTOFF:
		    done = true;
			state = CUTOFF;
			break;
		
		  case GRB_INFEASIBLE:
		    this->unfreeze_iis();
			break;

		  default:
			cout << "Conflict_resolver: unexpected status " << status << endl;
			exit(1);
			break;
		}
	}

	return state;
}


void Conflict_resolver::sync_values()
{
	for (auto& conf : this->confs) {
		if (!conf.active.curr) { continue; }

		conf.value.curr = conf.var.get(GRB_DoubleAttr_X) > 0.5;
	}

	for (auto& obj : this->objs) {
		obj.value = round(obj.var.get(GRB_DoubleAttr_X));
	}
}


void Conflict_resolver::unfreeze_iis()
{
	int status = this->model.get(GRB_IntAttr_Status);
	assert(status == GRB_INFEASIBLE);

	try {
		this->model.computeIIS();
	}
	catch (GRBException ex) {
		cout << "Conflict_resolver ISS: gurobi exception " << ex.getMessage() << 
			", code = " << ex.getErrorCode() << endl;
		exit(1);
	}

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
			// cout << "unfreeze IIS: " << conf.idx << endl;
			conf.unfreeze();
			break;
		}
	}

	assert(found);
}


void Conflict_resolver::init_model()
{
	this->model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
	this->model.set(GRB_DoubleAttr_MIPGap, 0.5);
}


Conflict_resolver::tim_t Conflict_resolver::get_obj_val()
{
	try {
		return round(this->model.get(GRB_DoubleAttr_ObjVal));
	}
	catch (GRBException ex) {
		cout << "Conflict_resolver obj: gurobi exception " << ex.getMessage() << 
			", code = " << ex.getErrorCode() << endl;
		return 0;
	}
}

void Conflict_resolver::remove_cons(Constr& cons)
{
	if (!cons.in_model) {
		return;
	}

	this->model.remove(cons.model);
	this->need_model_update = true;
	cons.in_model = false;
}


void Conflict_resolver::add_model_cycle_cons(Cycle_constr& cons)
{
	if (cons.in_model) {
		return;
	}

	GRBLinExpr expr = 0;
	for (auto x : cons.confs) {
		expr += this->confs[x.idx].to_expr(x.value);
	}

	cons.model = this->model.addConstr(expr <= cons.confs.size() - 1);
	cons.model.set(GRB_IntAttr_Lazy, 1);
}


void Conflict_resolver::add_model_path_cons(Path_constr& cons)
{
	if (cons.in_model) {
		return;
	}

	GRBLinExpr expr = 0;
	for (auto x : cons.confs) {
		expr += this->confs[x.idx].to_expr(x.value);
	}

	auto& obj = this->objs[cons.obj_idx];

	if (obj.is_bin) {
		cons.model = this->model.addConstr(expr - cons.confs.size() + 1 <= obj.var);
	}
	else {
		cons.model = this->model.addConstr((expr - cons.confs.size() + 1)*cons.delay <= obj.var);
	}

	cons.model.set(GRB_IntAttr_Lazy, 3);
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



