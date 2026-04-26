#include "conflict_resolver.hpp"


#include <format>

#include "utils/aux.hpp"
#include "utils/stl_print.hpp"

using namespace std;


Conflict_resolver::Conflict_resolver(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver), model(solver.grb_env)
{
	model.set(GRB_IntParam_LazyConstraints, 1);
	model.set(GRB_DoubleParam_MIPGap, 2e-3);
};


Conflict_resolver::~Conflict_resolver()
{

}


Preprocess::idx_pr Conflict_resolver::find_conflict_train(idx_t train)
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

	return chunk;
}

Preprocess::idx_pr Conflict_resolver::find_conflict_chunk(idx_t c)
{
	auto& time = this->slvr.chunk_mngr->time;
	auto& train = this->slvr.chunk_mngr->train_idx;

	idx_t r = this->slvr.chunk_mngr->res_idx[c];
	auto& res = this->slvr.chunk_mngr->res[r];

	auto& t_c = time[c];

	for (auto x = res.chunks; x < res.chunks + res.size; x++) {
		if (train[c] == train[*x]) {
			continue;
		}

		auto& t_x = time[*x];

		if ((t_x.start < t_c.end) && (t_x.end > t_c.start)) {
			return {*x, c};
		}

		if ((t_c.start < t_x.end) && (t_c.end > t_x.start)) {
			return {c, *x};
		}
	}

	return {IDX_MAX, IDX_MAX};
}



void Conflict_resolver::add_conflict(idx_pr chunk)
{
	assert(chunk.first < this->prepr.n_chunks());
	assert(chunk.second < this->prepr.n_chunks());
	
	auto& is_chunk_active = this->slvr.chunk_mngr->is_active;
	assert(is_chunk_active[chunk.first] && is_chunk_active[chunk.second]);

	bool default_val = false;
	if (chunk.first > chunk.second) {
		swap(chunk.first, chunk.second);
		default_val = true;
	}

	size_t idx = this->confs.size();
	assert(this->confs.size() < IDX_MAX);

	auto& train_idx = this->slvr.chunk_mngr->train_idx;

	idx_t t1 = train_idx[chunk.first];
	idx_t t2 = train_idx[chunk.second];

	assert(t1 < t2);

	this->slvr.link_graph.get_chain_conf(this->conf_chain, chunk);


	Conflict conf;
	conf.idx = (idx_t)idx;
	conf.frozen = false;
	conf.train = {t1, t2};
	conf.active = {true, false}; // curr = true, old = false
	conf.value = {default_val};
	conf.var = this->model.addVar(0, 1, 0, GRB_BINARY, format("conf_{}", idx));

	for (auto& x : this->conf_chain) {
		conf.chunks.push_back(x);
	}

	this->confs.push_back(conf);

	// cout << "conflict idx: " << conf.idx << 
	// 	", trains: (" << t1 << ", " << t2 <<
	// 	"), chunks: " << conf.chunks << endl;
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
		auto& conf = this->confs[x.idx];

		x.value = conf.value.curr;
		expr += conf.to_expr();
	}

	cycle_cons.model = this->model.addConstr(expr <= min_size - 1);
	cycle_cons.model.set(GRB_IntAttr_Lazy, 1);

	// this->purge_dominated_cycle(cycle_cons);

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
		auto& conf = this->confs[x.idx];
		x.value = conf.value.curr;

		expr += conf.to_expr();
	}
	

	if (is_bin) {
		path_cons.model = this->model.addConstr(expr - path_cons.confs.size() + 1 <= obj.var);
	}
	else {
		path_cons.model = this->model.addConstr((expr - path_cons.confs.size() + 1)*path_cons.delay <= obj.var);
	}
	path_cons.model.set(GRB_IntAttr_Lazy, 3);

	// this->purge_dominated_path(path_cons);
	this->path_constrs.push_back(path_cons);

	return true;
}


void Conflict_resolver::make_crit_path_count()
{
	auto& crit_mp = this->crit_path_count;

	crit_mp.clear();

	for (auto& obj : this->objs) {
		this->slvr.event_graph.get_critical_path(this->path, obj.prepr->level);
		for (auto& x : this->path) {
			if (x.e < EDG_MAX) {
				auto& conf = this->confs[x.e];
				crit_mp[conf.train.first] += 1;
				crit_mp[conf.train.second] += 1;
			}
		}
	}
}


void Conflict_resolver::purge_dominated_cycle(const Cycle_constr& cons)
{
	size_t i = 0;
	while (i < this->cycle_constrs.size()) {
		auto& x = this->cycle_constrs[i];

		if (x.has_conf_subset(cons.confs)) {
			this->model.remove(x.model);
			x = this->cycle_constrs.back();
			this->cycle_constrs.pop_back();
		}
		else {
			i++;
		}
	}
	
	i = 0;
	while (i < this->path_constrs.size()) {
		auto& x = this->path_constrs[i];

		if (x.has_conf_subset(cons.confs)) {
			this->model.remove(x.model);
			x = this->path_constrs.back();
			this->path_constrs.pop_back();
		}
		else {
			i++;
		}
	}
}


void Conflict_resolver::purge_dominated_path(const Path_constr& cons)
{
	size_t i = 0;
	while (i < this->path_constrs.size()) {
		auto& x = this->path_constrs[i];

		if ((x.obj_idx == cons.obj_idx) && (x.delay <= cons.delay) && x.has_conf_subset(cons.confs)) {
			this->model.remove(x.model);
			x = this->path_constrs.back();
			this->path_constrs.pop_back();
		}
		else {
			i++;
		}
	}
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


int Conflict_resolver::optimize_model()
{
	int status = GBR_EXCEPTION;
	bool done = false;
	while(!done) {
		try {
			this->model.update();
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
			break;

		  case GRB_CUTOFF:
		    done = true;
			return Solver::CUTOFF;
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

	return Solver::OPTIMAL;
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


void Conflict_resolver::freeze_conflicts()
{
	for (auto& conf : this->confs) {
		conf.freeze();
	}
}

void Conflict_resolver::clear_constrs(bool keep_critical)
{
	for (auto& constr : this->cycle_constrs) {
		this->model.remove(constr.model);
	}

	this->cycle_constrs.clear();

	if (keep_critical) {
		size_t i = 0;
		while (i < this->path_constrs.size()) {
			auto& x = this->path_constrs[i];

			bool required = false;
			if (x.is_bin || x.delay >= this->objs[x.obj_idx].value) {
				if (this->is_cons_conf_active(x)) {
					required = true;
				}
			}

			if (!required) {
				this->model.remove(x.model);
				x = this->path_constrs.back();
				this->path_constrs.pop_back();
			}
			else {
				i++;
			}
		}
	}
	else {
		for (auto& constr : this->path_constrs) {
			this->model.remove(constr.model);
		}
	}

	this->path_constrs.clear();
}


void Conflict_resolver::unfreeze_train_confs(idx_t train)
{
	for (auto& conf : this->confs) {
		if (conf.train.first == train || conf.train.second == train) {
			conf.unfreeze();
		}
	}
}


bool Conflict_resolver::is_cons_conf_active(const Constr& cons)
{
	for (auto& x : cons.confs) {
		if (this->confs[x.idx].value.curr != x.value) {
			return false;
		}
	}

	return true;
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

void Conflict_resolver::set_obj_ub(tim_t bound)
{
	this->model.set(GRB_DoubleParam_Cutoff, bound);
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

bool Conflict_resolver::Constr::has_conf_subset(const vector<Conf_assign>& subset) const
{
	size_t m = this->confs.size();
	size_t n = subset.size();

	if (m < n) {
		return false;
	}

	size_t i = 0;
	size_t j = 0;

	while (i < m && j < n) {
		auto& a = this->confs[i];
		auto& b = this->confs[j];

		if (a.idx < b.idx) {
			i++;
		}

		else if (a.idx > b.idx) {
			j++;
		}

		else { // equal
			if (a.value != b.value) {
				return false;
			}

			i++;
			j++;
		}
	}


	if (j == n ) {
		return true; // matched all elements of subset
	}
	

	return false;
}

